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
using namespace WEX::TestExecution;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace NetworkMidiTest
{
    namespace
    {
        LPWSTR CoTaskCopy(_In_ std::wstring const& text)
        {
            auto const bytes = (text.size() + 1) * sizeof(wchar_t);
            auto copy = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));

            if (copy != nullptr)
            {
                memcpy(copy, text.c_str(), bytes);
            }

            return copy;
        }
    }

    _Use_decl_annotations_
    STDMETHODIMP StandInDeviceManager::ActivateVirtualParentDevice(
        ULONG,
        const DEVPROPERTY*,
        const SW_DEVICE_CREATE_INFO* createInfo,
        LPWSTR* createdDeviceId)
    {
        if (createInfo == nullptr || createInfo->pszInstanceId == nullptr || createdDeviceId == nullptr) return E_INVALIDARG;

        *createdDeviceId = CoTaskCopy(std::wstring{ L"SWD\\MIDISRV\\" } + createInfo->pszInstanceId);

        return *createdDeviceId == nullptr ? E_OUTOFMEMORY : S_OK;
    }


    _Use_decl_annotations_
    STDMETHODIMP StandInDeviceManager::ActivateEndpoint(
        LPCWSTR,
        BOOL,
        MidiFlow,
        const PMIDIENDPOINTCOMMONPROPERTIES,
        ULONG,
        ULONG,
        const DEVPROPERTY*,
        const DEVPROPERTY*,
        const SW_DEVICE_CREATE_INFO* createInfo,
        LPWSTR* createdEndpointDeviceInterfaceId)
    {
        if (createdEndpointDeviceInterfaceId == nullptr) return E_POINTER;

        *createdEndpointDeviceInterfaceId = nullptr;

        if (createInfo == nullptr || createInfo->pszInstanceId == nullptr) return E_INVALIDARG;

        {
            std::scoped_lock lock{ m_activeLock };

            std::wstring const id{ createInfo->pszInstanceId };

            if (std::any_of(m_activeInstanceIds.begin(), m_activeInstanceIds.end(), [&id](auto const& active) { return _wcsicmp(active.c_str(), id.c_str()) == 0; }))
            {
                return S_FALSE;
            }

            m_activeInstanceIds.push_back(id);
        }

        *createdEndpointDeviceInterfaceId = CoTaskCopy(
            std::wstring{ L"\\\\?\\swd#midisrv#" } + createInfo->pszInstanceId + L"#{e7cce071-3c03-423f-88d3-f1045d756eb3}");

        return *createdEndpointDeviceInterfaceId == nullptr ? E_OUTOFMEMORY : S_OK;
    }


    _Use_decl_annotations_
    void StandInDeviceManager::Forget(LPCWSTR const instanceId)
    {
        if (instanceId == nullptr) return;

        std::wstring const id{ instanceId };

        std::scoped_lock lock{ m_activeLock };

        std::erase_if(m_activeInstanceIds, [&id](auto const& active)
            {
                // the transport may pass the id with or without the SWD\MIDISRV\ prefix
                return _wcsicmp(active.c_str(), id.c_str()) == 0 ||
                    (id.size() > active.size() && _wcsicmp(id.c_str() + (id.size() - active.size()), active.c_str()) == 0);
            });
    }

    GetClassObjectFunction LoadTransportForTest()
    {
        static GetClassObjectFunction getClassObject{ nullptr };

        if (getClassObject != nullptr)
        {
            return getClassObject;
        }

        // Keeps the multithreaded apartment alive for the rest of the process, as it is in the
        // service. The transport caches WinRT activation factories, and when the last apartment
        // in the process goes away between test methods, the DLLs behind those factories can be
        // unloaded, and the next use of one crashes.
        static CO_MTA_USAGE_COOKIE mtaUsage{ nullptr };

        if (mtaUsage == nullptr && FAILED(CoIncrementMTAUsage(&mtaUsage)))
        {
            Log::Error(L"Could not keep the multithreaded apartment alive");
            return nullptr;
        }

        std::wstring dllPath;
        String parameter;

        if (SUCCEEDED(RuntimeParameters::TryGetValue(L"TransportDll", parameter)) && !parameter.IsEmpty())
        {
            dllPath = static_cast<wchar_t const*>(parameter);
        }
        else
        {
            std::wstring modulePath(MAX_PATH, L'\0');
            modulePath.resize(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath.data(), static_cast<DWORD>(modulePath.size())));
            dllPath = modulePath.substr(0, modulePath.find_last_of(L'\\') + 1) + L"Midi2.NetworkMidiTransport.dll";
        }

        Log::Comment(String().Format(L"Transport: %s", dllPath.c_str()));

        // Never freed. The transport keeps state for the life of the process, as it does in the service.
        auto const module = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

        if (module == nullptr)
        {
            Log::Error(String().Format(L"The transport DLL did not load (%u)", GetLastError()));
            return nullptr;
        }

        getClassObject = reinterpret_cast<GetClassObjectFunction>(GetProcAddress(module, "DllGetClassObject"));

        return getClassObject;
    }


    InProcessTransport::~InProcessTransport()
    {
        Shutdown();
    }

    bool InProcessTransport::Start()
    {
        auto const getClassObject = LoadTransportForTest();

        if (getClassObject == nullptr) return false;

        wil::com_ptr_nothrow<IClassFactory> factory;

        if (FAILED(getClassObject(NetworkTransportClsid, __uuidof(IClassFactory), factory.put_void()))) return false;
        if (FAILED(factory->CreateInstance(nullptr, __uuidof(IMidiTransport), m_transport.put_void()))) return false;

        if (FAILED(m_transport->Activate(__uuidof(IMidiTransportConfigurationManager), m_configurationManager.put_void()))) return false;
        if (FAILED(m_configurationManager->Initialize(NetworkTransportClsid, m_deviceManager, nullptr))) return false;

        if (FAILED(m_transport->Activate(__uuidof(IMidiEndpointManager), m_endpointManager.put_void()))) return false;
        if (FAILED(m_endpointManager->Initialize(m_deviceManager, m_protocolManager))) return false;

        return true;
    }

    void InProcessTransport::Shutdown()
    {
        if (m_endpointManager != nullptr)
        {
            LOG_IF_FAILED(m_endpointManager->Shutdown());
        }

        if (m_configurationManager != nullptr)
        {
            LOG_IF_FAILED(m_configurationManager->Shutdown());
        }

        m_endpointManager.reset();
        m_configurationManager.reset();
        m_transport.reset();
    }

    _Use_decl_annotations_
    std::optional<winrt::Windows::Data::Json::JsonObject> InProcessTransport::Configure(std::wstring const& json)
    {
        if (m_configurationManager == nullptr) return std::nullopt;

        wil::unique_cotaskmem_string response;

        auto const hr = m_configurationManager->UpdateConfiguration(json.c_str(), response.put());

        if (FAILED(hr) || response == nullptr)
        {
            Log::Comment(String().Format(L"Configuration call failed (0x%08X): %s", hr, json.c_str()));
            return std::nullopt;
        }

        winrt::Windows::Data::Json::JsonObject result{ nullptr };

        if (!winrt::Windows::Data::Json::JsonObject::TryParse(response.get(), result))
        {
            return std::nullopt;
        }

        return result;
    }

    _Use_decl_annotations_
    uint16_t InProcessTransport::CreateHostAndWait(
        std::wstring const& entryIdentifier,
        std::wstring const& serviceInstanceName,
        bool const requireApproval)
    {
        std::wstring json =
            L"{\"create\":{\"hosts\":{\"" + entryIdentifier + L"\":{"
            L"\"name\":\"" + serviceInstanceName + L"\","
            L"\"productInstanceId\":\"" + serviceInstanceName + L"-piid\","
            L"\"serviceInstanceName\":\"" + serviceInstanceName + L"\","
            L"\"networkProtocol\":\"udp\","
            L"\"port\":\"auto\","
            L"\"enabled\":true,"
            L"\"advertise\":false,"
            L"\"createMidi1Ports\":false,"
            L"\"remoteClientPolicy\":\"" + std::wstring(requireApproval ? L"requireApproval" : L"allowAny") + L"\""
            L"}}}}";

        if (!Configure(json).has_value()) return 0;

        auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto response = Configure(L"{\"transportCommand\":{\"commandName\":\"enumerateHosts\"}}");

            if (response.has_value())
            {
                auto hosts = response->GetNamedArray(L"hosts", nullptr);

                if (hosts != nullptr)
                {
                    for (auto const& value : hosts)
                    {
                        auto host = value.GetObject();

                        if (_wcsicmp(std::wstring{ host.GetNamedString(L"entryIdentifier", L"") }.c_str(), entryIdentifier.c_str()) == 0 &&
                            host.GetNamedBoolean(L"hasStarted", false))
                        {
                            auto const portText = std::wstring{ host.GetNamedString(L"actualPort", L"") };

                            if (!portText.empty())
                            {
                                return static_cast<uint16_t>(std::stoul(portText));
                            }
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return 0;
    }

    _Use_decl_annotations_
    void InProcessTransport::RemoveHost(std::wstring const& entryIdentifier)
    {
        Configure(
            L"{\"transportCommand\":{\"commandName\":\"removeHost\",\"commandArguments\":{\"entryIdentifier\":\"" + entryIdentifier + L"\"}}}");
    }

    _Use_decl_annotations_
    bool InProcessTransport::ConnectDirect(std::wstring const& entryIdentifier, std::wstring const& address, uint16_t const port)
    {
        auto response = Configure(
            L"{\"transportCommand\":{"
            L"\"commandName\":\"connectDirect\","
            L"\"commandArguments\":{"
            L"\"entryIdentifier\":\"" + entryIdentifier + L"\","
            L"\"remoteAddress\":\"" + address + L"\","
            L"\"remotePort\":\"" + std::to_wstring(port) + L"\","
            L"\"umpEndpointName\":\"In Process Test Client\""
            L"}}}");

        return response.has_value() && response->GetNamedBoolean(L"success", false);
    }

    _Use_decl_annotations_
    void InProcessTransport::DisconnectClient(std::wstring const& entryIdentifier)
    {
        Configure(
            L"{\"transportCommand\":{\"commandName\":\"disconnectClient\",\"commandArguments\":{\"entryIdentifier\":\"" + entryIdentifier + L"\"}}}");
    }

    namespace
    {
        std::optional<winrt::Windows::Data::Json::JsonObject> FindEntry(
            _In_ std::optional<winrt::Windows::Data::Json::JsonObject> const& response,
            _In_ PCWSTR const arrayKey,
            _In_ std::wstring const& entryIdentifier)
        {
            if (!response.has_value()) return std::nullopt;

            auto entries = response->GetNamedArray(arrayKey, nullptr);

            if (entries == nullptr) return std::nullopt;

            for (auto const& value : entries)
            {
                auto entry = value.GetObject();

                if (_wcsicmp(std::wstring{ entry.GetNamedString(L"entryIdentifier", L"") }.c_str(), entryIdentifier.c_str()) == 0)
                {
                    return entry;
                }
            }

            return std::nullopt;
        }
    }

    _Use_decl_annotations_
    std::optional<winrt::Windows::Data::Json::JsonObject> InProcessTransport::FindClientEntry(std::wstring const& entryIdentifier)
    {
        return FindEntry(Configure(L"{\"transportCommand\":{\"commandName\":\"enumerateClients\"}}"), L"clients", entryIdentifier);
    }

    _Use_decl_annotations_
    std::optional<winrt::Windows::Data::Json::JsonObject> InProcessTransport::FindHostEntry(std::wstring const& entryIdentifier)
    {
        return FindEntry(Configure(L"{\"transportCommand\":{\"commandName\":\"enumerateHosts\"}}"), L"hosts", entryIdentifier);
    }


    RawUdpPeer::~RawUdpPeer()
    {
        if (m_socket != INVALID_SOCKET)
        {
            closesocket(m_socket);
        }

        if (m_winsockStarted)
        {
            WSACleanup();
        }
    }

    bool RawUdpPeer::Open()
    {
        WSADATA data{};

        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;

        m_winsockStarted = true;

        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        if (m_socket == INVALID_SOCKET) return false;

        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = 0;
        inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);

        if (bind(m_socket, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) return false;

        int length = sizeof(local);

        if (getsockname(m_socket, reinterpret_cast<sockaddr*>(&local), &length) != 0) return false;

        m_port = ntohs(local.sin_port);

        return true;
    }

    _Use_decl_annotations_
    bool RawUdpPeer::SendTo(uint16_t const port, std::vector<uint8_t> const& bytes)
    {
        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &remote.sin_addr);

        auto const sent = sendto(
            m_socket,
            reinterpret_cast<char const*>(bytes.data()),
            static_cast<int>(bytes.size()),
            0,
            reinterpret_cast<sockaddr*>(&remote),
            sizeof(remote));

        return sent == static_cast<int>(bytes.size());
    }

    _Use_decl_annotations_
    std::vector<RawUdpPeer::Received> RawUdpPeer::ReceiveFor(std::chrono::milliseconds const duration)
    {
        std::vector<Received> result;

        auto const deadline = std::chrono::steady_clock::now() + duration;

        while (true)
        {
            auto const remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());

            if (remaining.count() <= 0) break;

            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(m_socket, &readable);

            timeval timeout{};
            timeout.tv_sec = static_cast<long>(remaining.count() / 1000);
            timeout.tv_usec = static_cast<long>((remaining.count() % 1000) * 1000);

            if (select(0, &readable, nullptr, nullptr, &timeout) <= 0) break;

            std::vector<uint8_t> buffer(65536);
            sockaddr_in from{};
            int fromLength = sizeof(from);

            auto const received = recvfrom(m_socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);

            // a port which isn't listening answers with an ICMP error, which surfaces here
            if (received <= 0) continue;

            Received entry{};
            entry.FromPort = ntohs(from.sin_port);
            entry.Packet = ParsePacket(buffer.data(), static_cast<size_t>(received));

            result.push_back(std::move(entry));
        }

        return result;
    }

    _Use_decl_annotations_
    std::optional<RawUdpPeer::Received> RawUdpPeer::WaitFor(CommandCode const code, uint16_t const fromPort, std::chrono::milliseconds const timeout)
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto const remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());

            for (auto& entry : ReceiveFor((std::min)(remaining, std::chrono::milliseconds(100))))
            {
                if ((fromPort == 0 || entry.FromPort == fromPort) && entry.Packet.Contains(code))
                {
                    return entry;
                }
            }
        }

        return std::nullopt;
    }
}
