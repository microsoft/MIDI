// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// Loads the transport DLL into the test process, the way the service does, and hands it stand-ins
// for the service objects. Needs no service, no admin rights and no network peer. The transport
// sits beside this test DLL in the build output, unless /p:TransportDll=<path> names another one,
// which is how a test is run against an older build to show it fails there.

#include <string>
#include <optional>
#include <atomic>

namespace NetworkMidiTest
{
    // Midi2NetworkMidiTransport in Midi2NetworkMidiTransport.idl
    constexpr GUID NetworkTransportClsid{ 0xc95dcd1f, 0xcde3, 0x4c2d, { 0x91, 0x3c, 0x52, 0x8c, 0xb8, 0xa4, 0xcb, 0xe6 } };

    // Counts references and nothing else. Every stand-in outlives the transport, so it never
    // deletes itself.
    template <typename Interface>
    class CountedStandIn : public Interface
    {
    public:
        STDMETHODIMP QueryInterface(REFIID riid, void** object) override
        {
            if (object == nullptr) return E_POINTER;

            if (riid == __uuidof(IUnknown) || riid == __uuidof(Interface))
            {
                *object = static_cast<Interface*>(this);
                AddRef();
                return S_OK;
            }

            *object = nullptr;
            return E_NOINTERFACE;
        }

        STDMETHODIMP_(ULONG) AddRef() override { return ++m_references; }
        STDMETHODIMP_(ULONG) Release() override { return --m_references; }

        // Includes the one the test holds
        ULONG References() const { return m_references.load(); }

    private:
        std::atomic<ULONG> m_references{ 1 };
    };

    // Creates parent devices and endpoints, as far as the transport can tell, so a remote can
    // open a real session with it.
    class StandInDeviceManager : public CountedStandIn<IMidiDeviceManager>
    {
    public:
        STDMETHODIMP ActivateVirtualParentDevice(ULONG, const DEVPROPERTY*, const SW_DEVICE_CREATE_INFO* createInfo, LPWSTR* createdDeviceId) override;
        STDMETHODIMP DeactivateVirtualParentDevice(LPCWSTR) override { return S_OK; }

        // Hands back an interface id for the instance id it was given, so a session can open.
        // Nothing is created: the transport only keeps the id.
        STDMETHODIMP ActivateEndpoint(
            LPCWSTR,
            BOOL,
            MidiFlow,
            const PMIDIENDPOINTCOMMONPROPERTIES,
            ULONG,
            ULONG,
            const DEVPROPERTY*,
            const DEVPROPERTY*,
            const SW_DEVICE_CREATE_INFO* createInfo,
            LPWSTR* createdEndpointDeviceInterfaceId) override;

        STDMETHODIMP UpdateEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeleteEndpointProperties(LPCWSTR, ULONG, const DEVPROPERTY*) override { return S_OK; }
        STDMETHODIMP DeactivateEndpoint(LPCWSTR instanceId) override { Forget(instanceId); return S_OK; }
        STDMETHODIMP RemoveEndpoint(LPCWSTR instanceId) override { Forget(instanceId); return S_OK; }
        STDMETHODIMP RebuildMidi1PortsForEndpoint(LPCWSTR) override { return S_OK; }
        STDMETHODIMP UpdateTransportConfiguration(GUID, LPCWSTR, LPWSTR*) override { return E_NOTIMPL; }

    private:
        // Like the service, an instance id which is already active is S_FALSE with no interface id
        void Forget(_In_opt_ LPCWSTR const instanceId);

        std::mutex m_activeLock;
        std::vector<std::wstring> m_activeInstanceIds;
    };

    class StandInProtocolManager : public CountedStandIn<IMidiEndpointProtocolManager>
    {
    public:
        STDMETHODIMP DiscoverAndNegotiate(GUID, LPCWSTR, ENDPOINTPROTOCOLNEGOTIATIONPARAMS) override { return E_NOTIMPL; }
        STDMETHODIMP_(BOOL) IsEnabled() override { return FALSE; }
    };

    using GetClassObjectFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

    // Loads the transport once for the process and returns its DllGetClassObject. The DLL is never
    // freed: the transport keeps state for the life of the process, as it does in the service.
    GetClassObjectFunction LoadTransportForTest();

    // One transport, started in the test process with both managers initialized
    class InProcessTransport
    {
    public:
        InProcessTransport() = default;
        ~InProcessTransport();

        InProcessTransport(InProcessTransport const&) = delete;
        InProcessTransport& operator=(InProcessTransport const&) = delete;

        bool Start();

        // Shuts both managers down the way the service does. Safe to call twice.
        void Shutdown();

        // The transport's own JSON object, with "create", "transportSettings" or
        // "transportCommand" at the top level. Returns the response, or nullopt if the call failed.
        std::optional<winrt::Windows::Data::Json::JsonObject> Configure(_In_ std::wstring const& json);

        // Creates a host on an automatic port, not advertised, and waits for it to start.
        // Returns the port it is listening on, or 0.
        uint16_t CreateHostAndWait(
            _In_ std::wstring const& entryIdentifier,
            _In_ std::wstring const& serviceInstanceName,
            _In_ bool const requireApproval);

        // Removes a host, which also closes its connections
        void RemoveHost(_In_ std::wstring const& entryIdentifier);

        // A direct client entry, which the transport starts at once
        bool ConnectDirect(_In_ std::wstring const& entryIdentifier, _In_ std::wstring const& address, _In_ uint16_t const port);

        void DisconnectClient(_In_ std::wstring const& entryIdentifier);

        // The entry from enumerateClients or enumerateHosts with this entry identifier
        std::optional<winrt::Windows::Data::Json::JsonObject> FindClientEntry(_In_ std::wstring const& entryIdentifier);
        std::optional<winrt::Windows::Data::Json::JsonObject> FindHostEntry(_In_ std::wstring const& entryIdentifier);

        StandInDeviceManager& DeviceManager() { return *m_deviceManager; }

    private:
        // Never deleted. A transport which keeps a reference releases it at DLL unload, and that
        // must not crash the test process.
        StandInDeviceManager* m_deviceManager{ new StandInDeviceManager() };
        StandInProtocolManager* m_protocolManager{ new StandInProtocolManager() };

        wil::com_ptr_nothrow<IMidiTransport> m_transport;
        wil::com_ptr_nothrow<IMidiTransportConfigurationManager> m_configurationManager;
        wil::com_ptr_nothrow<IMidiEndpointManager> m_endpointManager;
    };

    // An unconnected UDP socket on 127.0.0.1, so it sees what arrives from every port. A remote
    // device which uses one port for everything it does is exactly this.
    class RawUdpPeer
    {
    public:
        RawUdpPeer() = default;
        ~RawUdpPeer();

        RawUdpPeer(RawUdpPeer const&) = delete;
        RawUdpPeer& operator=(RawUdpPeer const&) = delete;

        bool Open();
        uint16_t Port() const { return m_port; }

        bool SendTo(_In_ uint16_t const port, _In_ std::vector<uint8_t> const& bytes);

        struct Received
        {
            uint16_t FromPort{ 0 };
            ParsedPacket Packet{ };
        };

        // Every datagram which arrives within the time, oldest first
        std::vector<Received> ReceiveFor(_In_ std::chrono::milliseconds const duration);

        // The first datagram from fromPort, or from any port when fromPort is 0, which holds code
        std::optional<Received> WaitFor(_In_ CommandCode const code, _In_ uint16_t const fromPort, _In_ std::chrono::milliseconds const timeout);

    private:
        SOCKET m_socket{ INVALID_SOCKET };
        uint16_t m_port{ 0 };
        bool m_winsockStarted{ false };
    };
}
