// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

class MidiBleAdvertisedEndpointManager
{
public:
    HRESULT Initialize();

    MidiBleDevice* GetDevice(_In_ std::wstring endpointDeviceInterfaceId);

    HRESULT Cleanup();

private:


    void WatcherThreadWorker();

    //enumeration::DeviceWatcher m_Watcher{ nullptr };
    //bool m_enumerationCompleted{ false };

    BluetoothLEAdvertisementWatcher m_adWatcher{ nullptr };

    std::thread m_watcherThread{};

    // map of SWD ids to BLE endpoint devices
    std::map<std::wstring, MidiBleDevice> m_devices{};


    //winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Added_revoker m_DeviceAdded;
    //winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Removed_revoker m_DeviceRemoved;
    //winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Updated_revoker m_DeviceUpdated;
    //winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::Stopped_revoker m_DeviceStopped;
    //winrt::impl::consume_Windows_Devices_Enumeration_IDeviceWatcher<enumeration::IDeviceWatcher>::EnumerationCompleted_revoker m_DeviceEnumerationCompleted;


    //void OnDeviceAdded(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformation const& info);
    //void OnDeviceRemoved(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformationUpdate const& upd);
    //void OnDeviceUpdated(_In_ enumeration::DeviceWatcher, _In_ enumeration::DeviceInformationUpdate const& upd);

    //void OnDeviceWatcherStopped(_In_ enumeration::DeviceWatcher, _In_ foundation::IInspectable);
    //void OnEnumerationCompleted(_In_ enumeration::DeviceWatcher, _In_ foundation::IInspectable);

};
