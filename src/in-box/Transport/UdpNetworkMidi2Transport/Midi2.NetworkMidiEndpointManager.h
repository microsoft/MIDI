// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiEndpointMatchCriteria.h"



class CMidi2NetworkMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    STDMETHOD(InitiateDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId));

    // Defers the above to the background worker. Discovery and negotiation call into the
    // service and take service-wide locks, and endpoint creation raises PnP notifications whose
    // callbacks take those same locks. Doing that on the socket receive callback deadlocked the
    // whole receive path, so nothing on that thread may block on the service.
    HRESULT QueueDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId);

    // Shutting a connection down joins its two worker threads, so it cannot be done on the
    // socket receive callback without stalling every other datagram behind it.
    HRESULT QueueConnectionShutdown(_In_ std::shared_ptr<MidiNetworkConnection> connection);

    // endpoint for a remote client connected to this host
    STDMETHOD(CreateNewHostEndpointToRemoteClient(
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& parentHostDeviceInstanceId,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
        _In_ uint8_t const fallbackMidi1PortCount,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    // endpoint for this client connected to a remote host
    STDMETHOD(CreateNewClientEndpointToRemoteHost(
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
        _In_ uint8_t const fallbackMidi1PortCount,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    STDMETHOD(DeleteEndpoint(_In_ std::wstring deviceInstanceId));

    // The endpoint device id for a live endpoint matching these criteria, or empty. Used when a
    // customization arrives for something already connected, so a rename does not require the
    // connection to be torn down and rebuilt.
    winrt::hstring FindMatchingInstantiatedEndpoint(
        _In_ WindowsMidiServicesPluginConfigurationLib::MidiEndpointMatchCriteria& criteria);

    // Rewrites the fallback group terminal block and the MIDI 1.0 port name table for an endpoint
    // which is already up. Both properties are ones the service watches, so writing them is what
    // makes it re-sync the MIDI 1.0 ports: a changed port count takes effect, and a renamed
    // endpoint carries its new name down to its ports, without the connection being torn down.
    //
    // Pass zero for the count to keep whatever the endpoint already spans, which is what a rename
    // wants. Does nothing for an endpoint with no block, which is how a UMP-only one is told
    // apart without having to find the entry it came from.
    //
    // A caller which is applying a customization has to pass the name, because the endpoint's own
    // name does not reflect a custom name that was only just written. An empty string means the
    // custom name was cleared, so the device's own name is used again.
    HRESULT RefreshMidi1PortsForEndpoint(
        _In_ std::wstring const& endpointDeviceInterfaceId,
        _In_ uint8_t const fallbackMidi1PortCount,
        _In_ std::optional<std::wstring> const& portNameOverride = std::nullopt);

    STDMETHOD(StartRemoteHostWatcher)();
    STDMETHOD(StartBackgroundEndpointCreator)();

    void RefreshCalculatedLatencyProperties();
    STDMETHOD(StartBackgroundConnectionShutdown)();
    STDMETHOD(StartBackgroundNegotiation)();
    STDMETHOD(StartBackgroundHostEndpointCreation)();

    bool IsInitialized() { return m_initialized; }

    STDMETHOD(WakeupBackgroundEndpointCreatorThread)();
    STDMETHOD(WakeupBackgroundConnectionShutdownThread)();
    STDMETHOD(WakeupBackgroundNegotiationThread)();

    // Creating an endpoint takes over a second when several arrive at once, and it used to run
    // on the socket receive callback, so one burst of invitations stalled every datagram behind
    // it. The connection is answered with Invitation Reply: Pending and completed from here.
    HRESULT QueueHostEndpointCreation(
        _In_ std::shared_ptr<MidiNetworkHostConnection> connection,
        _In_ std::wstring const& clientUmpEndpointName,
        _In_ std::wstring const& clientProductInstanceId);

    // Created once per host and kept for the lifetime of the transport. There is deliberately no
    // matching delete: deactivating it leaves the instance id behind, which blocks activation and
    // leaves the host unable to build endpoints.
    HRESULT CreateParentDeviceForHost(
        _In_ winrt::hstring const& name,
        _In_ winrt::hstring const& id,
        _Inout_ std::wstring& createdNewDeviceInstanceId);


    HRESULT StartNewClient(
        _In_ std::shared_ptr<MidiNetworkClientDefinition> clientDefinition,
        _In_ winrt::hstring const& hostNameOrIPAddress,
        _In_ uint16_t const hostPort);


private:
    STDMETHOD(CreateNewEndpoint(
        _In_ MidiNetworkConnectionRole thisServiceRole,
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& parentId,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
        _In_ uint8_t const fallbackMidi1PortCount,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    // Shared by endpoint creation and the live refresh, so both produce the same blocks. The two
    // buffers are owned by the caller because the property entries point into them and must stay
    // valid until the device manager call returns.
    HRESULT BuildFallbackMidi1PortProperties(
        _In_ std::wstring const& portName,
        _In_ uint8_t const fallbackMidi1PortCount,
        _Inout_ std::vector<std::byte>& groupTerminalBlockData,
        _Inout_ WindowsMidiServicesNamingLib::MidiEndpointNameTable& nameTable,
        _Inout_ std::vector<DEVPROPERTY>& properties);

    ::WindowsMidiServicesInternal::MidiDnssdBrowser m_browser;

    void OnAdvertisedHostAdded(_In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service);
    void OnAdvertisedHostUpdated(_In_ ::WindowsMidiServicesInternal::MidiDnssdService const& service);
    void OnAdvertisedHostRemoved(_In_ std::wstring const& fullName, _In_ std::wstring const& deviceId);

    // Keyed by the same id Windows.Devices.Enumeration used, because configuration files store
    // it as the client match id.
    mutable wil::srwlock m_advertisedHostsLock;
    std::map<std::wstring, ::WindowsMidiServicesInternal::MidiDnssdService> m_foundAdvertisedHosts;

    // A host's virtual parent is created once and lives for the lifetime of the transport, so the
    // id activation handed back is remembered here rather than rebuilt. It is keyed on service
    // instance name because that is what the parent is named after, and a host which is stopped,
    // or removed and created again under the same name, has to be given the same parent back.
    mutable wil::srwlock m_hostParentDeviceIdsLock;
    std::map<std::wstring, std::wstring> m_hostParentDeviceIds;

    // What each live endpoint was created from, so a customization arriving later can be matched
    // back to it. The remote's identity is not otherwise recoverable from an endpoint id.
    struct CreatedEndpointRecord
    {
        winrt::hstring EndpointDeviceId;
        winrt::hstring DeviceInstanceId;
        winrt::hstring TransportSuppliedEndpointName;
        winrt::hstring ProductInstanceId;

        // Groups the fallback block spans, so a refresh can keep the width without reading the
        // block back out of the device store. Zero means the endpoint was built UMP-only and has
        // no MIDI 1.0 ports to rebuild.
        uint8_t FallbackMidi1PortCount{ 0 };
    };

    wil::critical_section m_createdEndpointsLock;
    std::vector<CreatedEndpointRecord> m_createdEndpoints;

    bool m_initialized{ false };

    GUID m_containerId{};
    GUID m_transportId{ };
    std::wstring m_clientParentDeviceInstanceId{};

    HRESULT CreateParentDeviceForClients();

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;

    // Last calculated latency written per endpoint, so the timer-driven refresh only writes on a
    // meaningful change. One millisecond at the 10 MHz clock the platform reports.
    std::map<std::wstring, uint64_t> m_lastWrittenLatencyTicks;
    uint64_t m_latencyWriteThresholdTicks{ ::WindowsMidiServicesInternal::GetMidiTimestampFrequency() / 1000 };
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;


    wil::slim_event_manual_reset m_backgroundEndpointCreatorThreadWakeup;
    HRESULT EndpointCreatorWorker(_In_ std::stop_token stopToken);

    wil::critical_section m_pendingNegotiationsLock;
    std::vector<std::wstring> m_pendingNegotiations;

    // Negotiation calls into the service and can block there indefinitely, so it gets its own
    // thread. On the endpoint creator thread a wedged negotiation stopped every configured
    // client from connecting.
    wil::slim_event_manual_reset m_backgroundNegotiationThreadWakeup;

    // Set by the worker as it leaves. Shutdown waits on this rather than joining the thread,
    // because a negotiation blocked in the service would otherwise hold up service stop.
    wil::slim_event_manual_reset m_negotiationThreadExitedEvent;

    HRESULT NegotiationWorker(_In_ std::stop_token stopToken);

    // Teardown runs on its own thread. Deactivating an endpoint is slow, and it has no ordering
    // relationship with starting a new connection, so sharing one worker meant a disconnecting
    // device delayed an unrelated device connecting.
    wil::slim_event_manual_reset m_backgroundConnectionShutdownThreadWakeup;
    HRESULT ConnectionShutdownWorker(_In_ std::stop_token stopToken);

    wil::critical_section m_pendingConnectionShutdownsLock;
    std::vector<std::shared_ptr<MidiNetworkConnection>> m_pendingConnectionShutdowns;

    struct PendingHostEndpointCreation
    {
        std::shared_ptr<MidiNetworkHostConnection> Connection;
        std::wstring ClientUmpEndpointName;
        std::wstring ClientProductInstanceId;

        // Time to Invitation Reply: Accepted is queue wait plus activation, and only the
        // measurement tells us which of the two to attack.
        std::chrono::steady_clock::time_point QueuedAt{ std::chrono::steady_clock::now() };
    };

    wil::slim_event_manual_reset m_backgroundHostEndpointCreationThreadWakeup;
    HRESULT HostEndpointCreationWorker(_In_ std::stop_token stopToken);

    wil::critical_section m_pendingHostEndpointCreationsLock;
    std::vector<PendingHostEndpointCreation> m_pendingHostEndpointCreations;

    // Must remain the last members. Members are destroyed in reverse declaration order, so this
    // guarantees the workers are joined before the wakeup events they wait on are destroyed.
    std::jthread m_backgroundEndpointCreatorThread;
    std::jthread m_backgroundConnectionShutdownThread;
    std::jthread m_backgroundNegotiationThread;
    std::jthread m_backgroundHostEndpointCreationThread;

};
