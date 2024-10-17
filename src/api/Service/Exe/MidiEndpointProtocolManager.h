// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


struct ProtocolNegotiationWorkerThreadEntry
{
    std::shared_ptr<std::thread> Thread;        // todo: consider making this a std::jthread and keeping stop token in this structure
    std::shared_ptr<CMidiEndpointProtocolWorker> Worker;
};

class CMidiEndpointProtocolManager : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiEndpointProtocolManagerInterface>
{
public:
    CMidiEndpointProtocolManager() = default;
    ~CMidiEndpointProtocolManager() {}

    STDMETHOD(Initialize)(
        _In_ std::shared_ptr<CMidiClientManager>& clientManager,
        _In_ std::shared_ptr<CMidiDeviceManager>& deviceManager,
        _In_ std::shared_ptr<CMidiSessionTracker>& sessionTracker
    );

    STDMETHOD(DiscoverAndNegotiate)(
        _In_ GUID transportId,
        _In_ LPCWSTR endpointDeviceInterfaceId,
        _In_ ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams,
        _In_ IMidiProtocolNegotiationCompleteCallback* negotiationCompleteCallback
        );


    HRESULT Shutdown();

private:
    GUID m_sessionId;

    std::shared_ptr<CMidiClientManager> m_clientManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;
    std::shared_ptr<CMidiSessionTracker> m_sessionTracker;


    std::mutex m_endpointWorkersMapMutex{};
    std::map<std::wstring, ProtocolNegotiationWorkerThreadEntry> m_endpointWorkers;

    HRESULT RemoveWorkerIfPresent(_In_ std::wstring endpointInterfaceId);

    winrt::Windows::Devices::Enumeration::DeviceWatcher m_watcher{ nullptr };
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<winrt::Windows::Devices::Enumeration::IDeviceWatcher>::Added_revoker m_DeviceAdded;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<winrt::Windows::Devices::Enumeration::IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<winrt::Windows::Devices::Enumeration::IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<winrt::Windows::Devices::Enumeration::IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<winrt::Windows::Devices::Enumeration::IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;

    HRESULT OnDeviceAdded(_In_ winrt::Windows::Devices::Enumeration::DeviceWatcher, _In_ winrt::Windows::Devices::Enumeration::DeviceInformation);
    HRESULT OnDeviceRemoved(_In_ winrt::Windows::Devices::Enumeration::DeviceWatcher, _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate);
    HRESULT OnDeviceUpdated(_In_ winrt::Windows::Devices::Enumeration::DeviceWatcher, _In_ winrt::Windows::Devices::Enumeration::DeviceInformationUpdate);
    HRESULT OnDeviceStopped(_In_ winrt::Windows::Devices::Enumeration::DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);
    HRESULT OnEnumerationCompleted(_In_ winrt::Windows::Devices::Enumeration::DeviceWatcher, _In_ winrt::Windows::Foundation::IInspectable);


};
