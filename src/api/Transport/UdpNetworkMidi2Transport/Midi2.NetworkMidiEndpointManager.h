// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class CMidi2NetworkMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManager*, _In_ IMidiEndpointProtocolManager*));
    STDMETHOD(Shutdown)();

    STDMETHOD(InitiateDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId));

    // endpoint for a remote client connected to this host
    STDMETHOD(CreateNewHostEndpointToRemoteClient(
        _In_ std::wstring const& configIdentifier,
        _In_ std::wstring const& parentHostDeviceInstanceId,
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _In_ bool umpOnly,
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
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    STDMETHOD(DeleteEndpoint(_In_ std::wstring deviceInstanceId));

    STDMETHOD(StartRemoteHostWatcher)();
    STDMETHOD(StartBackgroundEndpointCreator)();

    bool IsInitialized() { return m_initialized; }

    STDMETHOD(WakeupBackgroundEndpointCreatorThread)();

    HRESULT CreateParentDeviceForHost(
        _In_ winrt::hstring const& name,
        _In_ winrt::hstring const& id,
        _Inout_ std::wstring& createdNewDeviceInstanceId);

    HRESULT DeleteParentHostDevice(
        _In_ std::wstring const& deviceInstanceId);

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
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    enumeration::DeviceWatcher m_deviceWatcher{ nullptr };
    winrt::event_token m_deviceWatcherAddedToken;
    winrt::event_token m_deviceWatcherUpdatedToken;
    winrt::event_token m_deviceWatcherRemovedToken;
    winrt::event_token m_deviceWatcherStoppedToken;

    HRESULT OnDeviceWatcherAdded(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformation const& args);
    HRESULT OnDeviceWatcherUpdated(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherRemoved(_In_ enumeration::DeviceWatcher const&, _In_ enumeration::DeviceInformationUpdate const& args);
    HRESULT OnDeviceWatcherStopped(_In_ enumeration::DeviceWatcher const&, _In_ foundation::IInspectable const&);

    std::map<winrt::hstring, enumeration::DeviceInformation> m_foundAdvertisedHosts;

    bool m_initialized{ false };

    GUID m_containerId{};
    GUID m_transportId{ };
    std::wstring m_clientParentDeviceInstanceId{};

    HRESULT CreateParentDeviceForClients();

    wil::com_ptr_nothrow<IMidiDeviceManager> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManager> m_midiProtocolManager;

    HRESULT StartNewClient(
        _In_ std::shared_ptr<MidiNetworkClientDefinition> clientDefinition, 
        _In_ winrt::hstring const& hostNameOrIPAddress, 
        _In_ uint16_t const hostPort);


    std::jthread m_backgroundEndpointCreatorThread;
    //std::stop_token m_backgroundEndpointCreatorThreadStopToken;
    wil::slim_event_manual_reset m_backgroundEndpointCreatorThreadWakeup;
    HRESULT EndpointCreatorWorker(_In_ std::stop_token stopToken);

};
