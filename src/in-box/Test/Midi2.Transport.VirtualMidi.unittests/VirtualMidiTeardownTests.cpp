// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <atomic>
#include <memory>
#include <thread>

#include "VirtualMidiTeardownTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace TransportConfigTest;

namespace
{
    // {8FEAAD91-70E1-4A19-997A-377720A719C1}
    constexpr GUID VirtualMidiTransportId
    {
        0x8FEAAD91, 0x70E1, 0x4A19, { 0x99, 0x7A, 0x37, 0x77, 0x20, 0xA7, 0x19, 0xC1 }
    };

    constexpr wchar_t VirtualMidiTransportIdString[]{ L"{8FEAAD91-70E1-4A19-997A-377720A719C1}" };

    // the calling component id the other service-level transport tests use. The service only
    // traces it.
    constexpr GUID TestCallingComponent
    {
        0xc24cc593, 0xbc6b, 0x4726, { 0xb5, 0x52, 0xbe, 0xc8, 0x2d, 0xed, 0xb6, 0x8c }
    };

    // Far longer than a healthy close takes, including protocol negotiation timing out at its
    // configurable maximum of 50 seconds, so only a hung service gets here.
    constexpr DWORD HangTimeoutMilliseconds{ 60000 };


    // Neither connection is expected to receive anything that matters to these tests.
    class IgnoreMessagesCallback final : public IMidiCallback
    {
    public:
        STDMETHOD(QueryInterface)(_In_ REFIID riid, _COM_Outptr_ void** object) noexcept override
        {
            if (object == nullptr)
            {
                return E_POINTER;
            }

            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMidiCallback))
            {
                *object = static_cast<IMidiCallback*>(this);
                return S_OK;
            }

            *object = nullptr;
            return E_NOINTERFACE;
        }

        // Static lifetime, so a connection left behind on a hung thread can never outlive it.
        STDMETHOD_(ULONG, AddRef)() noexcept override { return 2; }
        STDMETHOD_(ULONG, Release)() noexcept override { return 1; }

        STDMETHOD(Callback)(
            _In_ MessageOptionFlags,
            _In_ PVOID,
            _In_ UINT,
            _In_ LONGLONG,
            _In_ LONGLONG) noexcept override
        {
            return S_OK;
        }
    };

    IgnoreMessagesCallback g_ignoreMessages;


    // Shared between the test thread and the thread doing the work. The test thread may give up
    // on a hung run and return while the worker still holds this, hence the shared_ptr.
    struct TeardownRun
    {
        explicit TeardownRun(_In_ bool openClientSide) : OpenClientSide{ openClientSide } { }

        bool const OpenClientSide;

        // String literals only. The test thread reads this after giving up on a hung run, when
        // nothing else here is safe to read.
        std::atomic<wchar_t const*> Step{ L"starting" };

        // Written by the worker, and read only after it sets Finished.
        bool Skipped{ false };
        HRESULT Result{ S_OK };
        wchar_t const* Message{ L"" };
        std::wstring Detail{ };
        std::wstring DeviceEndpointId{ };
        std::wstring ClientEndpointId{ };
        HRESULT DeviceCloseResult{ S_OK };
        ULONGLONG DeviceCloseMilliseconds{ 0 };
        HRESULT ClientCloseResult{ S_OK };

        wil::unique_event_nothrow Finished;
    };


    void Fail(_Inout_ TeardownRun& run, _In_ HRESULT result, _In_z_ wchar_t const* message)
    {
        run.Result = FAILED(result) ? result : E_FAIL;
        run.Message = message;
    }


    // The create response lists the device-side endpoint the service made, under createdDevices.
    std::wstring ReadCreatedDeviceEndpointId(_In_ std::wstring const& responseJson)
    {
        winrt::Windows::Data::Json::JsonObject response{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(responseJson, response))
        {
            return {};
        }

        auto createdDevices = response.GetNamedArray(L"createdDevices", nullptr);

        if (createdDevices == nullptr || createdDevices.Size() == 0)
        {
            return {};
        }

        return std::wstring{ createdDevices.GetObjectAt(0).GetNamedString(L"id", L"") };
    }


    // Both endpoints are named from the same unique id and differ only in this prefix. The SDK
    // finds the client side by association id instead, which needs device enumeration.
    std::wstring ClientEndpointIdFor(_In_ std::wstring const& deviceEndpointId)
    {
        constexpr std::wstring_view devicePrefix{ L"midiu_appdev_" };
        constexpr std::wstring_view clientPrefix{ L"midiu_apppub_" };

        auto position = deviceEndpointId.find(devicePrefix);

        if (position == std::wstring::npos)
        {
            return {};
        }

        std::wstring clientEndpointId{ deviceEndpointId };

        return clientEndpointId.replace(position, devicePrefix.size(), clientPrefix);
    }


    void RunTeardownSteps(_Inout_ TeardownRun& run)
    {
        run.Step = L"checking that the Virtual MIDI transport is available";

        // an empty create array is a no-op the transport still answers
        if (!SendTransportConfig(VirtualMidiTransportId, VirtualMidiTransportIdString, L"{\"create\":[]}").CallSucceeded)
        {
            run.Skipped = true;
            run.Message = L"Virtual MIDI transport is not available.";
            return;
        }

        run.Step = L"connecting to the service";

        wil::com_ptr_nothrow<IMidiTransport> serviceTransport;

        auto hr = CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&serviceTransport));
        if (FAILED(hr)) { Fail(run, hr, L"Could not create the MidiSrv transport."); return; }

        wil::com_ptr_nothrow<IMidiSessionTracker> sessionTracker;

        hr = serviceTransport->Activate(__uuidof(IMidiSessionTracker), (void**)&sessionTracker);
        if (FAILED(hr)) { Fail(run, hr, L"Could not activate the session tracker."); return; }

        hr = sessionTracker->Initialize();
        if (FAILED(hr)) { Fail(run, hr, L"Could not initialize the session tracker."); return; }

        GUID sessionId{ };

        hr = CoCreateGuid(&sessionId);
        if (FAILED(hr)) { Fail(run, hr, L"Could not create a session id."); return; }

        hr = sessionTracker->AddClientSession(sessionId, L"Virtual MIDI teardown test");
        if (FAILED(hr)) { Fail(run, hr, L"Could not register a session with the service."); return; }

        // declared before the connections, so it runs after they are closed
        auto removeSession = wil::scope_exit([&]
        {
            run.Step = L"closing the session";
            sessionTracker->RemoveClientSession(sessionId);
        });

        run.Step = L"creating the virtual device";

        // There is no command to remove a virtual device. Closing its device side is what removes
        // it, so a failure before the device side opens leaves it until the service restarts.
        auto created = SendTransportConfig(
            VirtualMidiTransportId,
            VirtualMidiTransportIdString,
            L"{\"create\":[{"
            L"\"associationIdentifier\":\"" + MakeGuidString() + L"\","
            L"\"name\":\"Service Teardown Test Virtual\","
            L"\"description\":\"Service test virtual device\","
            L"\"uniqueIdentifier\":\"" + MakeUniqueIdString() + L"\""
            L"}]}");

        if (!created.IsSuccess())
        {
            Fail(run, E_FAIL, L"The service did not create the virtual device.");
            run.Detail = created.Message + L" " + created.ResponseJson;
            return;
        }

        run.DeviceEndpointId = ReadCreatedDeviceEndpointId(created.ResponseJson);

        if (run.DeviceEndpointId.empty())
        {
            Fail(run, E_FAIL, L"The create response did not include the device-side endpoint id.");
            run.Detail = created.ResponseJson;
            return;
        }

        run.Step = L"opening the device side";

        TRANSPORTCREATIONPARAMS creationParams{ MessageOptionFlags_None, MidiDataFormats_UMP, TestCallingComponent };
        DWORD mmcssTaskId{ 0 };

        wil::com_ptr_nothrow<IMidiBidirectional> deviceSide;

        hr = serviceTransport->Activate(__uuidof(IMidiBidirectional), (void**)&deviceSide);
        if (FAILED(hr)) { Fail(run, hr, L"Could not activate the device-side connection."); return; }

        hr = deviceSide->Initialize(run.DeviceEndpointId.c_str(), &creationParams, &mmcssTaskId, &g_ignoreMessages, 0, sessionId);
        if (FAILED(hr)) { Fail(run, hr, L"Could not open the device side."); return; }

        // Opening the device side is what makes the service create the client side, and closing
        // it is the only way to remove the device, so this has to happen even if a later step fails.
        auto closeDeviceSide = wil::scope_exit([&]
        {
            run.Step = L"closing the device side after a failed step";
            deviceSide->Shutdown();
        });

        wil::com_ptr_nothrow<IMidiBidirectional> clientSide;

        if (run.OpenClientSide)
        {
            run.Step = L"opening the client side";

            run.ClientEndpointId = ClientEndpointIdFor(run.DeviceEndpointId);

            if (run.ClientEndpointId.empty())
            {
                Fail(run, E_FAIL, L"Could not work out the client-side endpoint id from the device-side endpoint id.");
                return;
            }

            hr = serviceTransport->Activate(__uuidof(IMidiBidirectional), (void**)&clientSide);
            if (FAILED(hr)) { Fail(run, hr, L"Could not activate the client-side connection."); return; }

            mmcssTaskId = 0;

            hr = clientSide->Initialize(run.ClientEndpointId.c_str(), &creationParams, &mmcssTaskId, &g_ignoreMessages, 0, sessionId);

            if (FAILED(hr))
            {
                clientSide.reset();
                Fail(run, hr, L"Could not open the client side.");
                return;
            }
        }

        // The step under test. This is the call that never returns when the service hangs while
        // removing the device.
        run.Step = L"closing the device side";
        closeDeviceSide.release();

        auto closeStart = GetTickCount64();
        run.DeviceCloseResult = deviceSide->Shutdown();
        run.DeviceCloseMilliseconds = GetTickCount64() - closeStart;

        if (clientSide)
        {
            // the service has already removed this endpoint, so only returning matters here
            run.Step = L"closing the client side";
            run.ClientCloseResult = clientSide->Shutdown();
        }
    }


    void RunTeardown(_Inout_ TeardownRun& run) noexcept
    {
        try
        {
            auto initializeResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            if (FAILED(initializeResult))
            {
                Fail(run, initializeResult, L"Could not initialize COM on the worker thread.");
            }
            else
            {
                auto uninitialize = wil::scope_exit([] { CoUninitialize(); });

                RunTeardownSteps(run);
            }
        }
        catch (...)
        {
            Fail(run, wil::ResultFromCaughtException(), L"An exception was thrown.");
        }

        run.Finished.SetEvent();
    }


    // Runs one teardown on its own thread. A hung service never returns from the close, so
    // waiting for it inline would stall the whole test pass instead of failing this test.
    bool FinishedBeforeTimeout(_In_ std::shared_ptr<TeardownRun> const& run)
    {
        std::thread worker([run] { RunTeardown(*run); });

        if (run->Finished.wait(HangTimeoutMilliseconds))
        {
            worker.join();
            return true;
        }

        // The thread is stuck in a call into the service. Keep this DLL loaded so the thread's
        // code is still there if that call ever returns, and let the thread go.
        HMODULE pinned{ nullptr };

        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&g_ignoreMessages),
            &pinned);

        worker.detach();

        return false;
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


    void VerifyTeardownDoesNotHangService(_In_ bool openClientSide)
    {
        auto run = std::make_shared<TeardownRun>(openClientSide);
        VERIFY_SUCCEEDED(run->Finished.create(wil::EventOptions::ManualReset));

        if (!FinishedBeforeTimeout(run))
        {
            VERIFY_FAIL(String().Format(
                L"The service did not return within %u seconds while %s, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
                HangTimeoutMilliseconds / 1000,
                run->Step.load()));

            return;
        }

        if (run->Skipped)
        {
            Log::Result(TestResults::Skipped, run->Message);
            return;
        }

        LogRun(*run);
        VERIFY_SUCCEEDED(run->Result, String().Format(L"%s (while %s)", run->Message, run->Step.load()));

        // The close can return while another service thread is still stuck, so check the service
        // still answers by adding and removing a second device.
        auto probe = std::make_shared<TeardownRun>(false);
        VERIFY_SUCCEEDED(probe->Finished.create(wil::EventOptions::ManualReset));

        if (!FinishedBeforeTimeout(probe))
        {
            VERIFY_FAIL(String().Format(
                L"The first virtual device was removed, but then the service did not return within %u seconds while %s for a second one, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
                HangTimeoutMilliseconds / 1000,
                probe->Step.load()));

            return;
        }

        LogRun(*probe);
        VERIFY_IS_FALSE(probe->Skipped);
        VERIFY_SUCCEEDED(probe->Result, String().Format(L"Second device: %s (while %s)", probe->Message, probe->Step.load()));
    }
}


void VirtualMidiTeardownTests::TestClosingDeviceSideDoesNotHangService()
{
    VerifyTeardownDoesNotHangService(false);
}


void VirtualMidiTeardownTests::TestClosingDeviceSideWithClientConnectedDoesNotHangService()
{
    VerifyTeardownDoesNotHangService(true);
}
