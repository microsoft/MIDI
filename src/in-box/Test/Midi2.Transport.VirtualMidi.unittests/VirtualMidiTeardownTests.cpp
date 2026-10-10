// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "VirtualMidiTeardownTests.h"
#include "VirtualDeviceTestSupport.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace VirtualDeviceTest;

namespace
{
    // Shared between the test thread and the worker doing the run, which the test thread may have
    // to abandon.
    struct TeardownRun
    {
        explicit TeardownRun(_In_ bool const openClientSide) : OpenClientSide{ openClientSide } { }

        bool const OpenClientSide;

        // String literals only. The test thread reads this after giving up on a hung run, when
        // nothing else here is safe to read.
        std::atomic<wchar_t const*> Step{ L"starting" };

        // Written by the run, and read only after it has finished.
        bool Skipped{ false };
        HRESULT Result{ S_OK };
        wchar_t const* Message{ L"" };
        std::wstring Detail{ };
        std::wstring DeviceEndpointId{ };
        std::wstring ClientEndpointId{ };
        HRESULT DeviceCloseResult{ S_OK };
        ULONGLONG DeviceCloseMilliseconds{ 0 };
        HRESULT ClientCloseResult{ S_OK };
    };


    void Fail(_Inout_ TeardownRun& run, _In_ HRESULT const result, _In_z_ wchar_t const* message)
    {
        run.Result = FAILED(result) ? result : E_FAIL;
        run.Message = message;
    }


    void RunTeardown(_Inout_ TeardownRun& run)
    {
        run.Step = L"checking that the Virtual MIDI transport is available";

        if (!VirtualAvailable())
        {
            run.Skipped = true;
            run.Message = L"Virtual MIDI transport is not available.";
            return;
        }

        run.Step = L"connecting to the service";

        MidiTest::ServiceSession session{ };

        auto hr = session.Open(L"Virtual MIDI teardown test");
        if (FAILED(hr)) { Fail(run, hr, L"Could not register a session with the service."); return; }

        run.Step = L"creating the virtual device";

        auto const device = CreateDevice(L"Service Teardown Test Virtual");

        if (device.DeviceEndpointId.empty())
        {
            Fail(run, E_FAIL, L"The service did not create the virtual device.");
            run.Detail = device.Detail;
            return;
        }

        run.DeviceEndpointId = device.DeviceEndpointId;

        run.Step = L"opening the device side";

        auto const deviceMessages = MidiTest::ReceivedMessages::Create(false);
        wil::com_ptr_nothrow<IMidiBidirectional> deviceSide{ };

        hr = session.Connect(device.DeviceEndpointId, deviceMessages.get(), deviceSide);
        if (FAILED(hr)) { Fail(run, hr, L"Could not open the device side."); return; }

        // Opening the device side is what makes the service create the client side, and closing
        // it is the only way to remove the device, so this has to happen even if a later step fails.
        auto closeDeviceSide = wil::scope_exit([&]
        {
            run.Step = L"closing the device side after a failed step";
            deviceSide->Shutdown();
        });

        auto const clientMessages = MidiTest::ReceivedMessages::Create(false);
        wil::com_ptr_nothrow<IMidiBidirectional> clientSide{ };

        if (run.OpenClientSide)
        {
            run.Step = L"opening the client side";
            run.ClientEndpointId = device.ClientEndpointId;

            if (run.ClientEndpointId.empty())
            {
                Fail(run, E_FAIL, L"Could not work out the client-side endpoint id from the device-side endpoint id.");
                return;
            }

            hr = session.Connect(run.ClientEndpointId, clientMessages.get(), clientSide);
            if (FAILED(hr)) { Fail(run, hr, L"Could not open the client side."); return; }
        }

        // The step under test. This is the call that never returns when the service hangs while
        // removing the device.
        run.Step = L"closing the device side";
        closeDeviceSide.release();

        auto const closeStart = GetTickCount64();
        run.DeviceCloseResult = deviceSide->Shutdown();
        run.DeviceCloseMilliseconds = GetTickCount64() - closeStart;

        if (clientSide)
        {
            // the service has already removed this endpoint, so only returning matters here
            run.Step = L"closing the client side";
            run.ClientCloseResult = clientSide->Shutdown();
        }

        run.Step = L"closing the session";
        session.Close();
    }


    std::shared_ptr<TeardownRun> RunGuardedTeardown(
        _In_ bool const openClientSide,
        _Out_ MidiTest::GuardedRunResult& guarded)
    {
        auto const run = std::make_shared<TeardownRun>(openClientSide);

        guarded = MidiTest::RunWithHangGuard([run] { RunTeardown(*run); });

        return run;
    }


    void LogRun(_In_ TeardownRun const& run)
    {
        Log::Comment(String().Format(L"Device side: %s", run.DeviceEndpointId.c_str()));

        if (run.OpenClientSide)
        {
            Log::Comment(String().Format(L"Client side: %s", run.ClientEndpointId.c_str()));
        }

        Log::Comment(String().Format(L"Closing the device side took %llu ms and returned 0x%08X",
            run.DeviceCloseMilliseconds, static_cast<unsigned int>(run.DeviceCloseResult)));

        if (run.OpenClientSide)
        {
            Log::Comment(String().Format(L"Closing the client side returned 0x%08X",
                static_cast<unsigned int>(run.ClientCloseResult)));
        }

        if (!run.Detail.empty())
        {
            Log::Comment(run.Detail.c_str());
        }
    }


    // The close can return while another service thread is still stuck, so the service also has
    // to be able to add and remove another device afterwards.
    void VerifyServiceStillAnswers(_In_z_ wchar_t const* after)
    {
        MidiTest::GuardedRunResult guarded{ };
        auto const probe = RunGuardedTeardown(false, guarded);

        if (!guarded.Finished)
        {
            VERIFY_FAIL(String().Format(
                L"After %s, the service did not return within %u seconds while %s for another virtual device, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
                after,
                MidiTest::HangTimeoutMilliseconds / 1000,
                probe->Step.load()));

            return;
        }

        VERIFY_SUCCEEDED(guarded.Result, String().Format(L"Exception while %s", probe->Step.load()));

        LogRun(*probe);
        VERIFY_IS_FALSE(probe->Skipped);
        VERIFY_SUCCEEDED(probe->Result, String().Format(L"Another device: %s (while %s)", probe->Message, probe->Step.load()));
    }


    void VerifyTeardownDoesNotHangService(_In_ bool const openClientSide)
    {
        MidiTest::GuardedRunResult guarded{ };
        auto const run = RunGuardedTeardown(openClientSide, guarded);

        if (!guarded.Finished)
        {
            VERIFY_FAIL(String().Format(
                L"The service did not return within %u seconds while %s, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
                MidiTest::HangTimeoutMilliseconds / 1000,
                run->Step.load()));

            return;
        }

        VERIFY_SUCCEEDED(guarded.Result, String().Format(L"Exception while %s", run->Step.load()));

        if (run->Skipped)
        {
            Log::Result(TestResults::Skipped, run->Message);
            return;
        }

        LogRun(*run);
        VERIFY_SUCCEEDED(run->Result, String().Format(L"%s (while %s)", run->Message, run->Step.load()));

        VerifyServiceStillAnswers(L"the first virtual device was removed");
    }


    // Sized so a service without the negotiation fix, which waits out the 5 second discovery
    // timeout for each close made during discovery, still finishes well inside the time limit.
    constexpr uint32_t ChurnThreadCount{ 3 };
    constexpr uint32_t ChurnDevicesPerThread{ 2 };

    struct ChurnRun
    {
        std::atomic<uint32_t> DevicesClosed{ 0 };
        std::atomic<uint32_t> Failures{ 0 };
        std::atomic<HRESULT> FirstFailure{ S_OK };
    };

    // Opens a new device's device side and closes it again straight away, while the service is
    // still running endpoint discovery on the client side.
    HRESULT OpenAndCloseOneDevice() noexcept try
    {
        MidiTest::ServiceSession session{ };
        RETURN_IF_FAILED(session.Open(L"Virtual MIDI teardown test, several at once"));

        auto const device = CreateDevice(L"Service Teardown Test Virtual");
        RETURN_HR_IF(E_FAIL, device.DeviceEndpointId.empty());

        auto const messages = MidiTest::ReceivedMessages::Create(false);
        wil::com_ptr_nothrow<IMidiBidirectional> deviceSide{ };

        RETURN_IF_FAILED(session.Connect(device.DeviceEndpointId, messages.get(), deviceSide));

        deviceSide->Shutdown();
        session.Close();

        return S_OK;
    }
    CATCH_RETURN();


    // Opening an endpoint takes the client manager lock and then the table lock; a teardown must not do the reverse.
    constexpr uint32_t RaceClosingDeviceCount{ 6 };
    constexpr uint32_t RaceOpeningAppCount{ 2 };
    constexpr uint32_t RaceDevicesPerOpeningApp{ 3 };

    struct OpenWhileClosingRun
    {
        // String literals only. The test thread reads this after giving up on a hung run.
        std::atomic<wchar_t const*> Step{ L"starting" };

        // set once every app is ready, so the opens and the closes start together
        wil::unique_event_nothrow Go{ };

        MidiTest::ServiceSession ClosingSession{ };
        std::vector<wil::com_ptr_nothrow<IMidiBidirectional>> DevicesToClose{ };

        std::atomic<uint32_t> Closed{ 0 };
        std::atomic<uint32_t> Opened{ 0 };
        std::atomic<uint32_t> Failures{ 0 };
        std::atomic<HRESULT> FirstFailure{ S_OK };

        void RecordFailure(_In_ HRESULT const result) noexcept
        {
            HRESULT expected{ S_OK };
            FirstFailure.compare_exchange_strong(expected, result);
            Failures++;
        }
    };


    HRESULT OpenNewDevice(
        _In_ MidiTest::ServiceSession& session,
        _Inout_ wil::com_ptr_nothrow<IMidiBidirectional>& deviceSide)
    {
        auto const device = CreateDevice(L"Service Teardown Test Virtual");
        RETURN_HR_IF(E_FAIL, device.DeviceEndpointId.empty());

        auto const messages = MidiTest::ReceivedMessages::Create(false);

        return session.Connect(device.DeviceEndpointId, messages.get(), deviceSide);
    }


    void CloseDevicesDuringRace(_In_ std::shared_ptr<OpenWhileClosingRun> const run) noexcept
    {
        MidiTest::MultithreadedApartment apartment{ };

        run->Go.wait();

        for (auto& deviceSide : run->DevicesToClose)
        {
            deviceSide->Shutdown();
            deviceSide.reset();

            run->Closed++;
        }
    }


    void OpenDevicesDuringRace(_In_ std::shared_ptr<OpenWhileClosingRun> const run) noexcept try
    {
        MidiTest::MultithreadedApartment apartment{ };
        MidiTest::ServiceSession session{ };
        std::vector<wil::com_ptr_nothrow<IMidiBidirectional>> opened{ };

        auto closeOpened = wil::scope_exit([&]
            {
                for (auto& deviceSide : opened)
                {
                    deviceSide->Shutdown();
                }

                opened.clear();
                session.Close();
            });

        THROW_IF_FAILED(session.Open(L"Virtual MIDI teardown test, opening while others close"));

        run->Go.wait();

        for (uint32_t i = 0; i < RaceDevicesPerOpeningApp; i++)
        {
            wil::com_ptr_nothrow<IMidiBidirectional> deviceSide{ };
            THROW_IF_FAILED(OpenNewDevice(session, deviceSide));

            opened.push_back(std::move(deviceSide));
            run->Opened++;
        }
    }
    catch (...)
    {
        run->RecordFailure(wil::ResultFromCaughtException());
    }
}


bool VirtualMidiTeardownTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool VirtualMidiTeardownTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void VirtualMidiTeardownTests::TestClosingDeviceSideDoesNotHangService()
{
    VerifyTeardownDoesNotHangService(false);
}


void VirtualMidiTeardownTests::TestClosingDeviceSideWithClientConnectedDoesNotHangService()
{
    VerifyTeardownDoesNotHangService(true);
}


void VirtualMidiTeardownTests::TestClosingManyDevicesAtOnceDoesNotHangService()
{
    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    auto const run = std::make_shared<ChurnRun>();

    auto const guarded = MidiTest::RunWithHangGuard([run]
        {
            std::vector<std::thread> apps{ };

            // joined even if starting one of them throws, because destroying a joinable thread ends the process
            auto joinAll = wil::scope_exit([&apps]
                {
                    for (auto& app : apps)
                    {
                        if (app.joinable())
                        {
                            app.join();
                        }
                    }
                });

            for (uint32_t i = 0; i < ChurnThreadCount; i++)
            {
                apps.emplace_back([run]
                    {
                        MidiTest::MultithreadedApartment apartment{ };

                        for (uint32_t j = 0; j < ChurnDevicesPerThread; j++)
                        {
                            auto const hr = OpenAndCloseOneDevice();

                            if (SUCCEEDED(hr))
                            {
                                run->DevicesClosed++;
                            }
                            else
                            {
                                HRESULT expected{ S_OK };
                                run->FirstFailure.compare_exchange_strong(expected, hr);
                                run->Failures++;
                            }
                        }
                    });
            }
        });

    if (!guarded.Finished)
    {
        VERIFY_FAIL(String().Format(
            L"The service did not return within %u seconds while %u apps were closing virtual devices at once (%u of %u closed), so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
            MidiTest::HangTimeoutMilliseconds / 1000,
            ChurnThreadCount,
            run->DevicesClosed.load(),
            ChurnThreadCount * ChurnDevicesPerThread));

        return;
    }

    VERIFY_SUCCEEDED(guarded.Result);
    VERIFY_ARE_EQUAL(run->Failures.load(), (uint32_t)0,
        String().Format(L"First failure 0x%08X", static_cast<unsigned int>(run->FirstFailure.load())));

    VerifyServiceStillAnswers(L"several virtual devices were removed at once");
}


void VirtualMidiTeardownTests::TestOpeningDevicesWhileOthersCloseDoesNotHangService()
{
    if (!VirtualAvailable())
    {
        Log::Result(TestResults::Skipped, L"Virtual MIDI transport is not available.");
        return;
    }

    auto const run = std::make_shared<OpenWhileClosingRun>();
    VERIFY_SUCCEEDED(run->Go.create(wil::EventOptions::ManualReset));

    auto const guarded = MidiTest::RunWithHangGuard([run]
        {
            // runs after the apps below have finished, and never if one of them hangs
            auto closeLeftovers = wil::scope_exit([&run]
                {
                    run->Step = L"closing the session";

                    for (auto& deviceSide : run->DevicesToClose)
                    {
                        if (deviceSide)
                        {
                            deviceSide->Shutdown();
                            deviceSide.reset();
                        }
                    }

                    run->ClosingSession.Close();
                });

            run->Step = L"opening the devices to close";

            THROW_IF_FAILED(run->ClosingSession.Open(L"Virtual MIDI teardown test, closing while others open"));

            for (uint32_t i = 0; i < RaceClosingDeviceCount; i++)
            {
                wil::com_ptr_nothrow<IMidiBidirectional> deviceSide{ };
                THROW_IF_FAILED(OpenNewDevice(run->ClosingSession, deviceSide));

                run->DevicesToClose.push_back(std::move(deviceSide));
            }

            run->Step = L"one app was closing virtual devices as two others opened new ones";

            std::vector<std::thread> apps{ };

            // starts them together, and joins them even if starting one of them throws
            auto startAndJoin = wil::scope_exit([&]
                {
                    run->Go.SetEvent();

                    for (auto& app : apps)
                    {
                        if (app.joinable())
                        {
                            app.join();
                        }
                    }
                });

            apps.emplace_back(CloseDevicesDuringRace, run);

            for (uint32_t i = 0; i < RaceOpeningAppCount; i++)
            {
                apps.emplace_back(OpenDevicesDuringRace, run);
            }
        });

    auto const openedTarget = RaceOpeningAppCount * RaceDevicesPerOpeningApp;

    if (!guarded.Finished)
    {
        VERIFY_FAIL(String().Format(
            L"The service did not return within %u seconds while %s (closed %u of %u, opened %u of %u), so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
            MidiTest::HangTimeoutMilliseconds / 1000,
            run->Step.load(),
            run->Closed.load(),
            RaceClosingDeviceCount,
            run->Opened.load(),
            openedTarget));

        return;
    }

    VERIFY_SUCCEEDED(guarded.Result, String().Format(L"Exception while %s", run->Step.load()));
    VERIFY_ARE_EQUAL(run->Failures.load(), (uint32_t)0,
        String().Format(L"First failure 0x%08X", static_cast<unsigned int>(run->FirstFailure.load())));
    VERIFY_ARE_EQUAL(run->Closed.load(), RaceClosingDeviceCount);
    VERIFY_ARE_EQUAL(run->Opened.load(), openedTarget);

    VerifyServiceStillAnswers(L"devices were opened while others were closing");
}
