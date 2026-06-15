// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiParentDeviceInformation.g.h"

#define STRING_DEVPKEY_Device_DriverDesc        L"{0a8b865dd-2e3d-4094-ad97-e593a7c75d6},4"
#define STRING_     L"{0a8b865dd-2e3d-4094-ad97-e593a7c75d6},5"
#define STRING_DEVPKEY_Device_DriverProvider    L"{0a8b865dd-2e3d-4094-ad97-e593a7c75d6},9"

#define STRING_DEVPKEY_Device_EnumeratorName    L"{a45c254e-df1c-4efd-8020-67d146a850e0},24"

#include <devpkey.h>

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiParentDeviceInformation : MidiParentDeviceInformationT<MidiParentDeviceInformation>
    {
        MidiParentDeviceInformation() = default;

        static midi2enum::MidiParentDeviceInformation CreateFromId(_In_ winrt::hstring const& deviceInstanceId) noexcept;
        
        static collections::IVectorView<midi2enum::MidiParentDeviceInformation> FindAllForContainer(_In_ winrt::guid const& containerId) noexcept;
        
        static collections::IVectorView<hstring> GetAdditionalPropertiesList() noexcept;



       // enumeration::DeviceInformation DeviceInformation() const noexcept { return m_deviceInformation; }

        winrt::hstring Id() const noexcept { return m_deviceInformation != nullptr ? internal::NormalizeDeviceInstanceIdHStringCopy(m_deviceInformation.Id()) : L""; }
        winrt::hstring Name() const noexcept { return m_name; }
        
        winrt::guid ContainerId() const noexcept { return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::guid>(L"System.Devices.ContainerId", m_deviceInformation, winrt::guid()); }
        winrt::hstring DeviceInstanceId() const noexcept { return internal::NormalizeDeviceInstanceIdHStringCopy(internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.DeviceInstanceId", m_deviceInformation, L"")); }

        winrt::hstring DriverInfPath() const noexcept { return GetStringProperty(DEVPKEY_Device_DriverInfPath); }
        winrt::hstring DriverKey() const noexcept { return GetStringProperty(DEVPKEY_Device_Driver); }
        winrt::hstring DriverProvider() const noexcept { return GetStringProperty(DEVPKEY_Device_DriverProvider); }
        winrt::hstring ServiceName() const noexcept { return GetStringProperty(DEVPKEY_Device_Service); }
        winrt::hstring DriverVersion() const noexcept { return GetStringProperty(DEVPKEY_Device_DriverVersion); }
        winrt::hstring EnumeratorName() const noexcept { return GetStringProperty(DEVPKEY_Device_EnumeratorName); }
        winrt::hstring ParentDeviceInstanceId() const noexcept { return internal::NormalizeDeviceInstanceIdHStringCopy(internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.Parent", m_deviceInformation, L"")); }
        winrt::hstring RelatedParentDeviceMediaInstanceId() const noexcept { return internal::NormalizeDeviceInstanceIdHStringCopy(m_relatedParentDeviceMediaInstanceId); }

        //winrt::hstring ModelName() const noexcept { return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.ModelName", m_deviceInformation, L""); }
        //winrt::hstring Manufacturer() const noexcept { return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(L"System.Devices.Manufacturer", m_deviceInformation, L""); }

        winrt::hstring ToString() const noexcept;

        uint16_t UsbVendorId() const noexcept { return m_usbVendorId; }
        uint16_t UsbProductId() const noexcept { return m_usbProductId; }
        winrt::hstring UsbSerialNumber() const noexcept { return m_usbSerialNumber; }

        collections::IMapView<winrt::hstring, foundation::IInspectable> Properties() { return m_properties.GetView(); }


        //collections::IMapView<winrt::hstring, foundation::IInspectable> Properties() { return m_properties.GetView(); }

        void InternalInitialize(_In_ enumeration::DeviceInformation const& deviceInformation) noexcept;

    private:
        // TODO: We need to actually parse and provide these
        uint16_t m_usbVendorId{};
        uint16_t m_usbProductId{};
        winrt::hstring m_usbSerialNumber{};

        winrt::hstring m_name{};
        winrt::hstring m_relatedParentDeviceMediaInstanceId{};      // this is the device associated with the MIDI driver itself, so it has the right driver info

        enumeration::DeviceInformation m_deviceInformation{ nullptr };

        collections::IMap<winrt::hstring, foundation::IInspectable> m_properties
        { winrt::multi_threaded_map<winrt::hstring, foundation::IInspectable>() };


        winrt::hstring GetStringProperty(_In_ DEVPROPKEY const& key) const noexcept
        {
            return internal::SafeGetSwdPropertyFromDeviceInformation<winrt::hstring>(internal::DevPropKeyToWinRTPropertyHString(key), m_deviceInformation, L"");
        }


    };
}
namespace winrt::Windows::Devices::Midi2::Enumeration::factory_implementation
{
    struct MidiParentDeviceInformation : MidiParentDeviceInformationT<MidiParentDeviceInformation, implementation::MidiParentDeviceInformation>
    {
    };
}
