// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.MidiParentDeviceInformation.g.h"

#include <devpkey.h>

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiParentDeviceInformation : MidiParentDeviceInformationT<MidiParentDeviceInformation>
    {
        MidiParentDeviceInformation() = default;

        static midi2enum::MidiParentDeviceInformation InternalCreateFromIds(
            _In_ winrt::hstring const& deviceInstanceId,
            _In_ winrt::hstring const& mediaDriverDeviceId) noexcept;
        
    //    static collections::IVectorView<midi2enum::MidiParentDeviceInformation> FindAllForContainer(_In_ winrt::guid const& containerId) noexcept;
        
        //static collections::IVectorView<hstring> GetAdditionalPropertiesList() noexcept;



       // enumeration::DeviceInformation DeviceInformation() const noexcept { return m_deviceInformation; }

        winrt::hstring Id() const noexcept { return m_id; }
        winrt::hstring Name() const noexcept { return m_name; }
        
        winrt::guid ContainerId() const noexcept { return m_containerId; }
        //winrt::hstring DeviceInstanceId() const noexcept { return m_deviceInstanceId; }

        winrt::hstring DriverInfPath() const noexcept { return m_driverInfPath; }
        winrt::hstring DriverKey() const noexcept { return m_driverKey; }
        winrt::hstring DriverProvider() const noexcept { return m_driverProvider; }
        winrt::hstring ServiceName() const noexcept { return m_serviceName; }
        winrt::hstring DriverVersion() const noexcept { return m_driverVersion; }
        winrt::hstring EnumeratorName() const noexcept { return m_enumeratorName; }
        winrt::hstring ParentDeviceInstanceId() const noexcept { return m_parentDeviceInstanceId; }
        winrt::hstring RelatedParentMediaDriverDeviceInstanceId() const noexcept { return m_relatedParentDeviceMediaInstanceId; }
        uint32_t ReportedDeviceIdsHash() const noexcept { return m_reportedDeviceIdsHash; }

        uint16_t UsbVendorId() const noexcept { return m_usbVendorId; }
        uint16_t UsbProductId() const noexcept { return m_usbProductId; }
        winrt::hstring UsbSerialNumber() const noexcept { return m_usbSerialNumber; }

        bool InternalInitialize(
            _In_ winrt::hstring const& actualParentDeviceInstanceId,
            _In_ winrt::hstring const& mediaDriverDeviceParentInstanceId) noexcept;

        winrt::hstring ToString() const noexcept;


    private:
        uint16_t m_usbVendorId{};
        uint16_t m_usbProductId{};
        winrt::hstring m_usbSerialNumber{};

        winrt::hstring m_name{};
        winrt::hstring m_id{};
        //winrt::hstring m_deviceInstanceId{};

        winrt::hstring m_relatedParentDeviceMediaInstanceId{};      // this is the device associated with the MIDI driver itself, so it has the right driver info
        winrt::hstring m_parentDeviceInstanceId{};

        winrt::guid m_containerId{};            // DEVPKEY_Device_Container System.Devices.ContainerId   
        winrt::hstring m_driverInfPath{};       // DEVPKEY_Device_DriverInfPath
        winrt::hstring m_driverKey{};           // DEVPKEY_Device_Driver
        winrt::hstring m_driverProvider{};      // DEVPKEY_Device_DriverProvider
        winrt::hstring m_serviceName{};         // DEVPKEY_Device_Service
        winrt::hstring m_driverVersion{};       // DEVPKEY_Device_DriverVersion
        winrt::hstring m_enumeratorName{};      // DEVPKEY_Device_EnumeratorName
        uint32_t m_reportedDeviceIdsHash{};     // DEVPKEY_Device_ReportedDeviceIdsHash

    };
}
//namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
//{
//    struct MidiParentDeviceInformation : MidiParentDeviceInformationT<MidiParentDeviceInformation, implementation::MidiParentDeviceInformation>
//    {
//    };
//}
