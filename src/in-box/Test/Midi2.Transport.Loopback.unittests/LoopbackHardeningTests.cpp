// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <atomic>
#include <thread>

// the service control manager, for the service's process id. wil adds its handle types for it
// when it is included again after winsvc.h.
#include <winsvc.h>
#include <wil\resource.h>

#include <Feature_Servicing_MIDI2LoopbackAndClientOpenHardening.h>

#include "LoopbackHardeningTests.h"
#include "LoopbackTestSupport.h"

// the transport's own error codes, so a test asserts the exact rejection reason
#include "..\..\Transport\LoopbackMidiTransport\loopback_transport_error_codes.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace LoopbackTest;

namespace json = ::winrt::Windows::Data::Json;

namespace
{
    // a MIDI 1.0 channel voice note on, one word
    constexpr uint32_t NoteOnAToB{ 0x20903C40 };
    constexpr uint32_t NoteOnBToA{ 0x20903E40 };

    // How many times the creation window test tries to open endpoint A before B exists. It is
    // about a quarter of a second wide, so one try nearly always does it.
    constexpr uint32_t CreationWindowAttempts{ 5 };


    bool HardeningTestable()
    {
        if (!Feature_Servicing_MIDI2LoopbackAndClientOpenHardening::IsEnabled())
        {
            Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackAndClientOpenHardening is disabled.");
            return false;
        }

        if (!LoopbackAvailable())
        {
            Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
            return false;
        }

        return true;
    }


    // With the rejection reasons gate off the service throws the response away, so there is no
    // code to read.
    uint32_t ExpectedRejectionErrorCode(_In_ uint32_t const errorCode)
    {
        if (Feature_Servicing_MIDI2TransportConfigRejectionReasons::IsEnabled())
        {
            return errorCode;
        }

        return 0;
    }

    uint32_t ReportedErrorCode(_In_ TransportConfigTest::ServiceConfigResult const& result)
    {
        json::JsonObject response{ nullptr };

        if (!json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, response) || response == nullptr)
        {
            return 0;
        }

        return static_cast<uint32_t>(response.GetNamedNumber(L"errorCode", 0));
    }


    struct Created
    {
        TransportConfigTest::ServiceConfigResult Result{ };
        std::wstring EndpointA{ };
        std::wstring EndpointB{ };
    };

    // Names carry the unique ids, because no two loopback endpoints may share a name.
    Created CreatePair(
        _In_ std::wstring const& associationId,
        _In_ std::wstring const& uniqueIdA,
        _In_ std::wstring const& uniqueIdB)
    {
        Created created{ };

        created.Result = SendLoopbackConfig(
            L"{\"create\":{\"" + TransportConfigTest::EscapeJsonString(associationId) + L"\":{"
            L"\"endpointA\":{"
            L"\"name\":\"Hardening " + TransportConfigTest::EscapeJsonString(uniqueIdA) + L" A\","
            L"\"uniqueIdentifier\":\"" + TransportConfigTest::EscapeJsonString(uniqueIdA) + L"\""
            L"},"
            L"\"endpointB\":{"
            L"\"name\":\"Hardening " + TransportConfigTest::EscapeJsonString(uniqueIdB) + L" B\","
            L"\"uniqueIdentifier\":\"" + TransportConfigTest::EscapeJsonString(uniqueIdB) + L"\""
            L"}}}}");

        json::JsonObject response{ nullptr };

        if (created.Result.IsSuccess() &&
            json::JsonObject::TryParse(winrt::hstring{ created.Result.ResponseJson }, response) &&
            response != nullptr)
        {
            created.EndpointA = std::wstring{ response.GetNamedString(L"endpointA", L"") };
            created.EndpointB = std::wstring{ response.GetNamedString(L"endpointB", L"") };
        }

        return created;
    }


    struct ListedPair
    {
        uint32_t Count{ 0 };
        std::wstring EndpointA{ };
        std::wstring EndpointB{ };
    };

    // How many list entries carry this association, and the endpoints of the last one.
    ListedPair GetListedPair(_In_ std::wstring const& associationId)
    {
        ListedPair listed{ };

        auto const result = SendLoopbackConfig(LR"({"transportCommand":{"commandName":"listEntries"}})");

        json::JsonObject response{ nullptr };

        if (!result.IsSuccess() ||
            !json::JsonObject::TryParse(winrt::hstring{ result.ResponseJson }, response) ||
            response == nullptr ||
            !response.HasKey(L"entries"))
        {
            return listed;
        }

        for (auto const& value : response.GetNamedArray(L"entries"))
        {
            if (value.ValueType() != json::JsonValueType::Object)
            {
                continue;
            }

            auto const entry = value.GetObject();

            if (!SameAssociation(entry.GetNamedString(L"associationIdentifier", L""), associationId))
            {
                continue;
            }

            listed.Count++;

            auto const a = entry.GetNamedObject(L"endpointA", nullptr);
            auto const b = entry.GetNamedObject(L"endpointB", nullptr);

            listed.EndpointA = a == nullptr ? L"" : std::wstring{ a.GetNamedString(L"endpointId", L"") };
            listed.EndpointB = b == nullptr ? L"" : std::wstring{ b.GetNamedString(L"endpointId", L"") };
        }

        return listed;
    }


    bool SameEndpoint(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
    {
        return !left.empty() && _wcsicmp(left.c_str(), right.c_str()) == 0;
    }


    // Asked of PnP directly, not of the service.
    bool EndpointInterfaceIsEnabled(_In_ std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            auto const info = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                endpointDeviceId,
                winrt::single_threaded_vector<winrt::hstring>(),
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

            return info != nullptr && info.IsEnabled();
        }
        catch (...)
        {
            return false;
        }
    }


    // 0 when the service is not running. Works without elevation.
    DWORD MidiServiceProcessId() noexcept
    {
        wil::unique_schandle manager{ OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

        if (!manager)
        {
            return 0;
        }

        wil::unique_schandle service{ OpenServiceW(manager.get(), L"midisrv", SERVICE_QUERY_STATUS) };

        if (!service)
        {
            return 0;
        }

        SERVICE_STATUS_PROCESS status{ };
        DWORD needed{ 0 };

        if (!QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed))
        {
            return 0;
        }

        return status.dwCurrentState == SERVICE_RUNNING ? status.dwProcessId : 0;
    }


    std::wstring Uppercase(_In_ std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), ::towupper);
        return value;
    }


    // Opens both ends in one session and checks one message gets through each way.
    void VerifyMidiCrossesBothWays(_In_ std::wstring const& endpointA, _In_ std::wstring const& endpointB)
    {
        MidiTest::ServiceSession session{ };
        VERIFY_SUCCEEDED(session.Open(L"Loopback hardening tests"));

        auto receivedA = MidiTest::ReceivedMessages::Create();
        auto receivedB = MidiTest::ReceivedMessages::Create();
        VERIFY_IS_NOT_NULL(receivedA.get());
        VERIFY_IS_NOT_NULL(receivedB.get());

        wil::com_ptr_nothrow<IMidiBidirectional> a{ };
        wil::com_ptr_nothrow<IMidiBidirectional> b{ };

        auto close = wil::scope_exit([&]
            {
                if (!MidiTest::CloseConnectionWithHangGuard(a) || !MidiTest::CloseConnectionWithHangGuard(b))
                {
                    session.Abandon();
                }
            });

        VERIFY_SUCCEEDED(session.Connect(endpointA, receivedA.get(), a));
        VERIFY_SUCCEEDED(session.Connect(endpointB, receivedB.get(), b));

        VERIFY_SUCCEEDED(MidiTest::SendWords(a.get(), { NoteOnAToB }));
        VERIFY_IS_TRUE(receivedB->WaitForMessage({ NoteOnAToB }, MessageWaitMilliseconds), L"A to B did not arrive");

        VERIFY_SUCCEEDED(MidiTest::SendWords(b.get(), { NoteOnBToA }));
        VERIFY_IS_TRUE(receivedA->WaitForMessage({ NoteOnBToA }, MessageWaitMilliseconds), L"B to A did not arrive");
    }
}


bool LoopbackHardeningTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool LoopbackHardeningTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void LoopbackHardeningTests::TestCreateWithAssociationIdInUseIsRejected()
{
    if (!HardeningTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    auto const associationId = TransportConfigTest::MakeGuidString();
    auto const firstUniqueId = TransportConfigTest::MakeUniqueIdString().substr(0, 20);
    auto const secondUniqueId = TransportConfigTest::MakeUniqueIdString().substr(0, 20);

    std::vector<std::wstring> toRemove{ associationId };

    auto cleanup = wil::scope_exit([&]
        {
            for (auto const& id : toRemove)
            {
                try { RemoveLoopback(id); } catch (...) {}
            }
        });

    auto const first = CreatePair(associationId, firstUniqueId, firstUniqueId);
    VERIFY_IS_TRUE(first.Result.IsSuccess(), first.Result.ResponseJson.c_str());

    // same id in other letter case, which the transport treats as the same id
    auto const second = CreatePair(Uppercase(associationId), secondUniqueId, secondUniqueId);
    VERIFY_IS_TRUE(second.Result.CallSucceeded);
    VERIFY_IS_FALSE(second.Result.IsSuccess(), second.Result.ResponseJson.c_str());
    VERIFY_ARE_EQUAL(ExpectedRejectionErrorCode(LOOPBACK_ERROR_CODE_INVALID_ASSOCIATION_ID), ReportedErrorCode(second.Result));

    // the first loopback is still the one listed, and still carries MIDI
    auto const listed = GetListedPair(associationId);
    VERIFY_ARE_EQUAL(listed.Count, (uint32_t)1);
    VERIFY_IS_TRUE(SameEndpoint(listed.EndpointA, first.EndpointA), listed.EndpointA.c_str());
    VERIFY_IS_TRUE(SameEndpoint(listed.EndpointB, first.EndpointB), listed.EndpointB.c_str());

    VerifyMidiCrossesBothWays(first.EndpointA, first.EndpointB);

    VERIFY_IS_TRUE(RemoveLoopback(associationId).IsSuccess());
    toRemove.clear();

    // Nothing was left behind holding the refused loopback's unique ids. Before the fix its
    // endpoints were, and creating them again failed until the service restarted.
    auto const recreatedAssociationId = TransportConfigTest::MakeGuidString();
    toRemove.push_back(recreatedAssociationId);

    auto const recreated = CreatePair(recreatedAssociationId, secondUniqueId, secondUniqueId);
    VERIFY_IS_TRUE(recreated.Result.IsSuccess(), recreated.Result.ResponseJson.c_str());
}


void LoopbackHardeningTests::TestConcurrentCreatesWithOneAssociationIdCreateOneLoopback()
{
    if (!HardeningTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    constexpr uint32_t creatorCount{ 6 };

    auto const associationId = TransportConfigTest::MakeGuidString();

    std::vector<std::wstring> uniqueIds{ };

    for (uint32_t i = 0; i < creatorCount; i++)
    {
        uniqueIds.push_back(TransportConfigTest::MakeUniqueIdString().substr(0, 20));
    }

    std::vector<std::wstring> toRemove{ associationId };

    auto cleanup = wil::scope_exit([&]
        {
            for (auto const& id : toRemove)
            {
                try { RemoveLoopback(id); } catch (...) {}
            }
        });

    wil::unique_event_nothrow start{ };
    VERIFY_SUCCEEDED(start.create(wil::EventOptions::ManualReset));

    std::atomic<uint32_t> created{ 0 };
    std::vector<std::thread> creators{ };

    for (uint32_t i = 0; i < creatorCount; i++)
    {
        creators.emplace_back([&, i]
            {
                start.wait();

                try
                {
                    if (CreatePair(associationId, uniqueIds[i], uniqueIds[i]).Result.IsSuccess())
                    {
                        created++;
                    }
                }
                catch (...)
                {
                }
            });
    }

    start.SetEvent();

    for (auto& creator : creators)
    {
        creator.join();
    }

    VERIFY_ARE_EQUAL(created.load(), (uint32_t)1);
    VERIFY_ARE_EQUAL(GetListedPair(associationId).Count, (uint32_t)1);

    VERIFY_IS_TRUE(RemoveLoopback(associationId).IsSuccess());
    toRemove.clear();

    // every unique id is free again, so no endpoint was left behind by a create that lost
    for (auto const& uniqueId : uniqueIds)
    {
        auto const recreatedAssociationId = TransportConfigTest::MakeGuidString();
        toRemove.push_back(recreatedAssociationId);

        auto const recreated = CreatePair(recreatedAssociationId, uniqueId, uniqueId);
        VERIFY_IS_TRUE(recreated.Result.IsSuccess(), recreated.Result.ResponseJson.c_str());
    }
}


void LoopbackHardeningTests::TestConcurrentRenamesAndListsKeepTheServiceRunning()
{
    if (!HardeningTestable())
    {
        return;
    }

    if (!Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2EndpointCustomizationEnhancements is disabled.");
        return;
    }

    if (!Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackMuteAndList is disabled.");
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    constexpr uint32_t iterations{ 200 };

    auto const associationId = TransportConfigTest::MakeGuidString();
    auto const uniqueId = TransportConfigTest::MakeUniqueIdString().substr(0, 20);

    auto cleanup = wil::scope_exit([&] { try { RemoveLoopback(associationId); } catch (...) {} });

    auto const pair = CreatePair(associationId, uniqueId, uniqueId);
    VERIFY_IS_TRUE(pair.Result.IsSuccess(), pair.Result.ResponseJson.c_str());

    // Long names, so each rename has to reallocate the string a list may be copying.
    std::wstring const longA = L"Hardening rename " + uniqueId + std::wstring(80, L'a');
    std::wstring const longB = L"Hardening rename " + uniqueId + std::wstring(80, L'b');

    std::atomic<uint32_t> failedRenames{ 0 };
    std::atomic<uint32_t> failedLists{ 0 };

    auto renamer = [&](_In_ uint32_t const offset)
        {
            for (uint32_t i = 0; i < iterations; i++)
            {
                auto const& nameA = ((i + offset) % 2 == 0) ? longA : longB;
                auto const& nameB = ((i + offset) % 2 == 0) ? longB : longA;

                auto const result = SendLoopbackConfig(
                    L"{\"update\":["
                    L"{\"match\":{\"endpointDeviceId\":\"" + TransportConfigTest::EscapeJsonString(pair.EndpointA) + L"\"},"
                    L"\"customProperties\":{\"name\":\"" + nameA + L"\",\"description\":\"" + nameB + L"\"}},"
                    L"{\"match\":{\"endpointDeviceId\":\"" + TransportConfigTest::EscapeJsonString(pair.EndpointB) + L"\"},"
                    L"\"customProperties\":{\"name\":\"" + nameB + L"\",\"description\":\"" + nameA + L"\"}}"
                    L"]}");

                if (!result.CallSucceeded)
                {
                    failedRenames++;
                }
            }
        };

    auto lister = [&]()
        {
            for (uint32_t i = 0; i < iterations; i++)
            {
                auto const result = SendLoopbackConfig(LR"({"transportCommand":{"commandName":"listEntries"}})");

                if (!result.IsSuccess())
                {
                    failedLists++;
                }
            }
        };

    std::thread renamerOne([&] { renamer(0); });
    std::thread renamerTwo([&] { renamer(1); });
    std::thread listerOne(lister);
    std::thread listerTwo(lister);

    renamerOne.join();
    renamerTwo.join();
    listerOne.join();
    listerTwo.join();

    VERIFY_ARE_EQUAL(failedRenames.load(), (uint32_t)0);
    VERIFY_ARE_EQUAL(failedLists.load(), (uint32_t)0);

    // the service is still there, and the loopback still carries MIDI
    VERIFY_IS_TRUE(LoopbackAvailable());
    VerifyMidiCrossesBothWays(pair.EndpointA, pair.EndpointB);
}


void LoopbackHardeningTests::TestSideBUniqueIdContainingSideAPrefixCarriesMidiBothWays()
{
    if (!HardeningTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    auto const associationId = TransportConfigTest::MakeGuidString();
    auto const uniqueId = TransportConfigTest::MakeUniqueIdString().substr(0, 20);

    auto cleanup = wil::scope_exit([&] { try { RemoveLoopback(associationId); } catch (...) {} });

    // allowed characters, and within the length limit
    auto const pair = CreatePair(associationId, uniqueId, L"MIDIU_LOOP_A_" + uniqueId);
    VERIFY_IS_TRUE(pair.Result.IsSuccess(), pair.Result.ResponseJson.c_str());

    VerifyMidiCrossesBothWays(pair.EndpointA, pair.EndpointB);
}


void LoopbackHardeningTests::TestEndpointOpenedWhileItsLoopbackIsCreatedReceivesMidi()
{
    if (!HardeningTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    bool openedInWindow{ false };

    for (uint32_t attempt = 0; attempt < CreationWindowAttempts && !openedInWindow; attempt++)
    {
        auto const associationId = TransportConfigTest::MakeGuidString();
        auto const uniqueId = Uppercase(TransportConfigTest::MakeUniqueIdString().substr(0, 20));

        // The id the service gives endpoint A, worked out in advance so it can be opened the
        // moment it appears. Checked against the real one below.
        std::wstring const expectedEndpointA =
            std::wstring{ L"\\\\?\\SWD#MIDISRV#MIDIU_LOOP_A_" } + uniqueId + L"#" + STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI;

        auto removeLoopback = wil::scope_exit([&] { try { RemoveLoopback(associationId); } catch (...) {} });

        std::atomic<bool> createReturned{ false };
        Created created{ };

        std::thread creator([&]
            {
                try
                {
                    created = CreatePair(associationId, uniqueId, uniqueId);
                }
                catch (...)
                {
                }

                createReturned = true;
            });

        MidiTest::ServiceSession session{ };
        auto const sessionOpened = session.Open(L"Loopback hardening tests");

        auto receivedA = MidiTest::ReceivedMessages::Create();
        auto receivedB = MidiTest::ReceivedMessages::Create();

        wil::com_ptr_nothrow<IMidiBidirectional> a{ };
        wil::com_ptr_nothrow<IMidiBidirectional> b{ };

        auto close = wil::scope_exit([&]
            {
                if (!MidiTest::CloseConnectionWithHangGuard(a) || !MidiTest::CloseConnectionWithHangGuard(b))
                {
                    session.Abandon();
                }
            });

        bool openedBeforeCreateReturned{ false };

        if (SUCCEEDED(sessionOpened) && receivedA != nullptr)
        {
            // Waits for endpoint A to appear before asking the service to open it. The service must
            // never be asked to open an endpoint id that does not exist: in builds tested on
            // Oct 23 2026 that took the whole service down.
            while (!createReturned)
            {
                if (EndpointInterfaceIsEnabled(expectedEndpointA))
                {
                    if (SUCCEEDED(session.Connect(expectedEndpointA, receivedA.get(), a)))
                    {
                        // only counts if B still did not exist, which the create returning tells us
                        openedBeforeCreateReturned = !createReturned;
                    }

                    break;
                }

                Sleep(2);
            }
        }

        creator.join();

        VERIFY_SUCCEEDED(sessionOpened);
        VERIFY_IS_TRUE(created.Result.IsSuccess(), created.Result.ResponseJson.c_str());
        VERIFY_IS_TRUE(SameEndpoint(created.EndpointA, expectedEndpointA), created.EndpointA.c_str());

        if (!openedBeforeCreateReturned)
        {
            Log::Comment(String().Format(L"Attempt %u did not open endpoint A before its loopback was complete. Trying again.", attempt + 1));
            continue;
        }

        openedInWindow = true;

        VERIFY_IS_NOT_NULL(receivedB.get());
        VERIFY_SUCCEEDED(session.Connect(created.EndpointB, receivedB.get(), b));

        VERIFY_SUCCEEDED(MidiTest::SendWords(b.get(), { NoteOnBToA }));
        VERIFY_IS_TRUE(receivedA->WaitForMessage({ NoteOnBToA }, MessageWaitMilliseconds), L"B to A did not arrive at the endpoint opened early");

        VERIFY_SUCCEEDED(MidiTest::SendWords(a.get(), { NoteOnAToB }));
        VERIFY_IS_TRUE(receivedB->WaitForMessage({ NoteOnAToB }, MessageWaitMilliseconds), L"A to B did not arrive");
    }

    VERIFY_IS_TRUE(openedInWindow, L"never managed to open endpoint A while its loopback was being created");
}


void LoopbackHardeningTests::TestOpeningAnEndpointNobodyCreatedFailsWithoutStoppingTheService()
{
    if (!HardeningTestable())
    {
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    // Several of each, because the crash came from the second failure on the same RPC thread.
    constexpr uint32_t attemptsPerId{ 5 };

    std::vector<std::wstring> const missingIds
    {
        // the right shape for a loopback endpoint, but never created
        std::wstring{ L"\\\\?\\SWD#MIDISRV#MIDIU_LOOP_A_" } + Uppercase(TransportConfigTest::MakeUniqueIdString()) + L"#" + STRING_DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI,

        // not an endpoint id at all
        L"not an endpoint id",
    };

    auto const servicePidBefore = MidiServiceProcessId();
    VERIFY_ARE_NOT_EQUAL(servicePidBefore, (DWORD)0, L"the MIDI service is not running");

    MidiTest::ServiceSession session{ };
    VERIFY_SUCCEEDED(session.Open(L"Loopback hardening tests"));

    auto received = MidiTest::ReceivedMessages::Create();
    VERIFY_IS_NOT_NULL(received.get());

    // Collected rather than verified one by one, so on a build without the fix the run goes on to
    // show the service going down, not only the first wrong result.
    uint32_t wrongResults{ 0 };

    for (auto const& missingId : missingIds)
    {
        for (uint32_t attempt = 0; attempt < attemptsPerId; attempt++)
        {
            wil::com_ptr_nothrow<IMidiBidirectional> connection{ };

            auto const hr = session.Connect(missingId, received.get(), connection);

            Log::Comment(String().Format(L"Open of '%s', attempt %u: 0x%08X", missingId.c_str(), attempt + 1, hr));

            if (SUCCEEDED(hr))
            {
                MidiTest::CloseConnectionWithHangGuard(connection);
            }

            if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
            {
                wrongResults++;
            }
        }
    }

    // the same service process is still running, and still opens a real endpoint
    VERIFY_ARE_EQUAL(MidiServiceProcessId(), servicePidBefore, L"the MIDI service stopped or restarted");
    VERIFY_ARE_EQUAL(wrongResults, (uint32_t)0, L"opens which did not fail with ERROR_NOT_FOUND");

    auto const associationId = TransportConfigTest::MakeGuidString();
    auto const uniqueId = TransportConfigTest::MakeUniqueIdString().substr(0, 20);

    auto cleanup = wil::scope_exit([&] { try { RemoveLoopback(associationId); } catch (...) {} });

    auto const pair = CreatePair(associationId, uniqueId, uniqueId);
    VERIFY_IS_TRUE(pair.Result.IsSuccess(), pair.Result.ResponseJson.c_str());

    VerifyMidiCrossesBothWays(pair.EndpointA, pair.EndpointB);

    VERIFY_ARE_EQUAL(MidiServiceProcessId(), servicePidBefore);
}
