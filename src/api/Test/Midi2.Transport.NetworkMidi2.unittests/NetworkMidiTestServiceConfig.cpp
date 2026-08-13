// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <WexTestClass.h>

using namespace WEX::Common;
using namespace WEX::Logging;

namespace NetworkMidiTest
{
    namespace
    {
        // {C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}
        constexpr GUID NetworkTransportId
        {
            0xC95DCD1F, 0xCDE3, 0x4C2D, { 0x91, 0x3C, 0x52, 0x8C, 0xB8, 0xA4, 0xCB, 0xE6 }
        };

        constexpr wchar_t NetworkTransportIdString[]{ L"{C95DCD1F-CDE3-4C2D-913C-528CB8A4CBE6}" };


        // MidiSrvUpdateConfiguration hands the payload to GetTransportSettingsFromJsonString,
        // which looks for endpointTransportPluginSettings and treats each child key as a
        // transport GUID, then insists on exactly one. So the transport-specific object has to
        // be wrapped twice even though Initialize() was already told which transport this is.
        std::wstring WrapForTransport(std::wstring const& transportObjectJson)
        {
            return std::wstring{ L"{\"endpointTransportPluginSettings\":{\"" } +
                NetworkTransportIdString +
                L"\":" +
                transportObjectJson +
                L"}}";
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


    std::wstring MakeEntryIdentifier()
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


    ServiceConfigResult SendNetworkTransportConfig(std::wstring const& configJson)
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

            auto initializeResult = configManager->Initialize(NetworkTransportId, nullptr, nullptr);

            if (FAILED(initializeResult))
            {
                result.Message = String().Format(
                    L"Configuration manager Initialize failed (0x%08X). Is the network transport enabled?", initializeResult);
                return result;
            }

            wil::unique_cotaskmem_string responseString;

            auto wrapped = WrapForTransport(configJson);

            auto updateResult = configManager->UpdateConfiguration(wrapped.c_str(), responseString.put());

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


    bool IsServiceAvailable()
    {
        // an empty create section is a no-op the transport still answers
        auto result = SendNetworkTransportConfig(L"{\"create\":{}}");

        if (!result.CallSucceeded && !result.Message.empty())
        {
            Log::Comment(String().Format(L"Service probe failed: %s", result.Message.c_str()));
        }

        return result.CallSucceeded;
    }


    ServiceConfigResult CreateDirectClient(
        std::wstring const& entryIdentifier,
        std::wstring const& hostNameOrAddress,
        uint16_t const port,
        bool const createMidi1Ports)
    {
        // directPort is read with GetNamedString, so it has to be quoted
        std::wstring json =
            L"{\"create\":{\"clients\":{\"" + EscapeJsonString(entryIdentifier) + L"\":{"
            L"\"networkProtocol\":\"udp\","
            L"\"enabled\":true,"
            L"\"createMidi1Ports\":" + std::wstring(createMidi1Ports ? L"true" : L"false") + L","
            L"\"match\":{"
            L"\"directHostNameOrIP\":\"" + EscapeJsonString(hostNameOrAddress) + L"\","
            L"\"directPort\":\"" + std::to_wstring(port) + L"\""
            L"}}}}}";

        Log::Comment(String().Format(L"Creating client %s -> %s:%u", entryIdentifier.c_str(), hostNameOrAddress.c_str(), port));

        return SendNetworkTransportConfig(json);
    }


    ServiceConfigResult DisconnectClient(std::wstring const& entryIdentifier)
    {
        std::wstring json =
            L"{\"transportCommand\":{"
            L"\"commandName\":\"disconnectClient\","
            L"\"commandArguments\":{"
            L"\"entryIdentifier\":\"" + EscapeJsonString(entryIdentifier) + L"\""
            L"}}}";

        return SendNetworkTransportConfig(json);
    }


    ServiceConfigResult EnumerateClients()
    {
        return SendNetworkTransportConfig(
            L"{\"transportCommand\":{\"commandName\":\"enumerateClients\"}}");
    }


    ServiceConfigResult SetDirectConnectionScanInterval(uint32_t const milliseconds)
    {
        std::wstring json =
            L"{\"transportSettings\":{\"directConnectionScanInterval\":" + std::to_wstring(milliseconds) + L"}}";

        return SendNetworkTransportConfig(json);
    }


    ServiceConfigResult CreateHost(
        std::wstring const& entryIdentifier,
        std::wstring const& umpEndpointName,
        std::wstring const& productInstanceId,
        std::wstring const& serviceInstanceName,
        bool const requireApproval)
    {
        // advertise is off so these short-lived test hosts do not appear over mDNS and get
        // picked up by anything else on the network.
        // serviceInstanceName is set explicitly because it otherwise defaults to the machine
        // name, which the machine's own host is already using, and the parent device is created
        // from it. That collision is now rejected outright by the configuration manager.
        std::wstring json =
            L"{\"create\":{\"hosts\":{\"" + EscapeJsonString(entryIdentifier) + L"\":{"
            L"\"name\":\"" + EscapeJsonString(umpEndpointName) + L"\","
            L"\"productInstanceId\":\"" + EscapeJsonString(productInstanceId) + L"\","
            L"\"serviceInstanceName\":\"" + EscapeJsonString(serviceInstanceName) + L"\","
            L"\"networkProtocol\":\"udp\","
            L"\"port\":\"auto\","
            L"\"enabled\":true,"
            L"\"advertise\":false,"
            L"\"remoteClientPolicy\":\"" +
                std::wstring(requireApproval ? L"requireApproval" : L"allowAny") + L"\""
            L"}}}}";

        Log::Comment(String().Format(
            L"Creating host %s (%s) serviceInstanceName=%s policy=%s",
            entryIdentifier.c_str(),
            umpEndpointName.c_str(),
            serviceInstanceName.c_str(),
            requireApproval ? L"requireApproval" : L"allowAny"));

        return SendNetworkTransportConfig(json);
    }


    ServiceConfigResult StartHost(std::wstring const& entryIdentifier)
    {
        std::wstring json =
            L"{\"transportCommand\":{"
            L"\"commandName\":\"startHost\","
            L"\"commandArguments\":{"
            L"\"entryIdentifier\":\"" + EscapeJsonString(entryIdentifier) + L"\""
            L"}}}";

        return SendNetworkTransportConfig(json);
    }


    ServiceConfigResult StopHost(std::wstring const& entryIdentifier)
    {
        std::wstring json =
            L"{\"transportCommand\":{"
            L"\"commandName\":\"stopHost\","
            L"\"commandArguments\":{"
            L"\"entryIdentifier\":\"" + EscapeJsonString(entryIdentifier) + L"\""
            L"}}}";

        return SendNetworkTransportConfig(json);
    }


    ServiceConfigResult EnumerateHosts()
    {
        return SendNetworkTransportConfig(
            L"{\"transportCommand\":{\"commandName\":\"enumerateHosts\"}}");
    }


    ServiceConfigResult GetPendingRemoteClients()
    {
        return SendNetworkTransportConfig(
            L"{\"transportCommand\":{\"commandName\":\"getPendingRemoteClients\"}}");
    }


    ServiceConfigResult RemoveHost(std::wstring const& entryIdentifier)
    {
        std::wstring json =
            L"{\"transportCommand\":{"
            L"\"commandName\":\"removeHost\","
            L"\"commandArguments\":{"
            L"\"entryIdentifier\":\"" + EscapeJsonString(entryIdentifier) + L"\""
            L"}}}";

        return SendNetworkTransportConfig(json);
    }


    namespace
    {
        ServiceConfigResult RemoteClientDecision(
            std::wstring const& verb,
            std::wstring const& hostEntryIdentifier,
            std::wstring const& umpEndpointName,
            std::wstring const& productInstanceId,
            std::wstring const& scope)
        {
            std::wstring json =
                L"{\"transportCommand\":{"
                L"\"commandName\":\"" + verb + L"\","
                L"\"commandArguments\":{"
                L"\"entryIdentifier\":\"" + EscapeJsonString(hostEntryIdentifier) + L"\","
                L"\"umpEndpointName\":\"" + EscapeJsonString(umpEndpointName) + L"\","
                L"\"productInstanceId\":\"" + EscapeJsonString(productInstanceId) + L"\","
                L"\"scope\":\"" + EscapeJsonString(scope) + L"\""
                L"}}}";

            Log::Comment(String().Format(
                L"%s %s (%s) scope=%s",
                verb.c_str(),
                umpEndpointName.c_str(),
                productInstanceId.c_str(),
                scope.c_str()));

            return SendNetworkTransportConfig(json);
        }
    }


    ServiceConfigResult ApproveRemoteClient(
        std::wstring const& hostEntryIdentifier,
        std::wstring const& umpEndpointName,
        std::wstring const& productInstanceId,
        std::wstring const& scope)
    {
        return RemoteClientDecision(
            L"approveRemoteClient", hostEntryIdentifier, umpEndpointName, productInstanceId, scope);
    }


    ServiceConfigResult DenyRemoteClient(
        std::wstring const& hostEntryIdentifier,
        std::wstring const& umpEndpointName,
        std::wstring const& productInstanceId,
        std::wstring const& scope)
    {
        return RemoteClientDecision(
            L"denyRemoteClient", hostEntryIdentifier, umpEndpointName, productInstanceId, scope);
    }
}
