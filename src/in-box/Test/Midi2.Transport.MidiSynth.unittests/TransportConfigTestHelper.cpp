// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace WEX::Common;
using namespace WEX::Logging;

namespace TransportConfigTest
{
    namespace
    {
        // Nothing else in this test DLL uses COM, so the calling thread has not been
        // initialized. Without this every CoCreateInstance fails with CO_E_NOTINITIALIZED and
        // the tests silently skip as though the service were absent.
        struct ComApartmentScope
        {
            ComApartmentScope()
            {
                auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

                // RPC_E_CHANGED_MODE means the thread is already in a different apartment,
                // which is usable but not ours to undo
                m_owned = SUCCEEDED(hr);
            }

            ~ComApartmentScope()
            {
                if (m_owned)
                {
                    CoUninitialize();
                }
            }

            ComApartmentScope(ComApartmentScope const&) = delete;
            ComApartmentScope& operator=(ComApartmentScope const&) = delete;

        private:
            bool m_owned{ false };
        };
    }


    std::wstring EscapeJsonString(std::wstring const& value)
    {
        std::wstring result;

        for (auto const& c : value)
        {
            if (c == L'"' || c == L'\\')
            {
                result.push_back(L'\\');
            }

            result.push_back(c);
        }

        return result;
    }


    std::wstring MakeGuidString()
    {
        GUID guid{ };

        if (FAILED(CoCreateGuid(&guid)))
        {
            return L"{00000000-0000-0000-0000-000000000000}";
        }

        wchar_t buffer[64]{ };
        StringFromGUID2(guid, buffer, ARRAYSIZE(buffer));

        return std::wstring{ buffer };
    }


    std::wstring MakeUniqueIdString()
    {
        auto value = MakeGuidString();

        value.erase(
            std::remove_if(value.begin(), value.end(), [](wchar_t c) { return !iswalnum(c); }),
            value.end());

        return value;
    }


    ServiceConfigResult SendRawServiceConfig(GUID const& transportId, std::wstring const& rawPayload)
    {
        ServiceConfigResult result{ };

        // declared first so it outlives every COM pointer below
        ComApartmentScope comScope;

        try
        {
            wil::com_ptr_nothrow<IMidiTransport> serviceTransport;

            auto createResult = CoCreateInstance(
                __uuidof(Midi2MidiSrvTransport),
                nullptr,
                CLSCTX_ALL,
                __uuidof(IMidiTransport),
                (void**)&serviceTransport);

            if (FAILED(createResult) || serviceTransport == nullptr)
            {
                result.Message = String().Format(
                    L"Could not create the MidiSrv transport (0x%08X). Is the service installed?", createResult);
                return result;
            }

            wil::com_ptr_nothrow<IMidiTransportConfigurationManager> configManager;

            auto activateResult = serviceTransport->Activate(
                __uuidof(IMidiTransportConfigurationManager),
                (void**)&configManager);

            if (FAILED(activateResult) || configManager == nullptr)
            {
                result.Message = String().Format(
                    L"Could not activate the transport configuration manager (0x%08X).", activateResult);
                return result;
            }

            auto initializeResult = configManager->Initialize(transportId, nullptr, nullptr);

            if (FAILED(initializeResult))
            {
                result.Message = String().Format(
                    L"Configuration manager Initialize failed (0x%08X). Is the transport enabled?", initializeResult);
                return result;
            }

            wil::unique_cotaskmem_string responseString;

            auto updateResult = configManager->UpdateConfiguration(rawPayload.c_str(), responseString.put());

            result.CallSucceeded = SUCCEEDED(updateResult);

            if (responseString)
            {
                result.ResponseJson = responseString.get();

                // Deliberately a substring test rather than a JSON parse. The response shape is
                // the transport's own, and a test which parsed it with the transport's helpers
                // would stop being an independent check.
                result.ReportedSuccess = result.ResponseJson.find(L"\"success\":true") != std::wstring::npos;
            }

            if (!result.CallSucceeded)
            {
                result.Message = L"UpdateConfiguration returned a failure HRESULT.";
            }
        }
        catch (...)
        {
            result.Message = L"Exception while sending configuration to the service.";
        }

        return result;
    }


    ServiceConfigResult SendTransportConfig(
        GUID const& transportId,
        std::wstring const& transportIdString,
        std::wstring const& transportObjectJson)
    {
        // MidiSrvUpdateConfiguration hands the payload to GetTransportSettingsFromJsonString,
        // which looks for endpointTransportPluginSettings and treats each child key as a
        // transport GUID, then insists on exactly one. So the transport-specific object has to
        // be wrapped twice even though Initialize() was already told which transport this is.
        std::wstring wrapped =
            std::wstring{ L"{\"endpointTransportPluginSettings\":{\"" } +
            transportIdString +
            L"\":" +
            transportObjectJson +
            L"}}";

        return SendRawServiceConfig(transportId, wrapped);
    }


    bool IsTransportAvailable(GUID const& transportId, std::wstring const& transportIdString)
    {
        // an empty create section is a no-op the transport still answers
        auto result = SendTransportConfig(transportId, transportIdString, L"{\"create\":{}}");

        if (!result.CallSucceeded && !result.Message.empty())
        {
            Log::Comment(String().Format(L"Transport probe failed: %s", result.Message.c_str()));
        }

        return result.CallSucceeded;
    }
}
