// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiParentDeviceInformation.h"
#include "MidiParentDeviceInformation.g.cpp"

namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{

    // VID, PID, and Serial number, and a proper name all come from the actual hardware parent device, 
    // which is often a composite device. But the driver properties come from the media 
    // interface device with the &MI_xx (xx is a number, usually 00) in the name

    _Use_decl_annotations_
    bool MidiParentDeviceInformation::InternalInitialize(
        winrt::hstring const& actualParentDeviceInstanceId,
        winrt::hstring const& mediaDriverDeviceParentInstanceId) noexcept
    {
        // Get vid/pid/serial

        if (internal::IsStandardUsbDeviceInstanceId(actualParentDeviceInstanceId.c_str()))
        {
            uint16_t vid{ 0 };
            uint16_t pid{ 0 };
            std::wstring serial{ };

            auto hr = internal::ParseUsbDeviceInstanceIntoVidPidSerial(actualParentDeviceInstanceId.c_str(), vid, pid, serial);

            if (SUCCEEDED(hr))
            {
                m_usbVendorId = vid;
                m_usbProductId = pid;
                m_usbSerialNumber = winrt::hstring(serial);
            }
        }

        // get properties from the parent device
        auto pnpinfoParent = internal::MidiPnpDeviceInfo::CreateFromInstanceId(actualParentDeviceInstanceId.c_str());
        if (!pnpinfoParent)
        {
            // this is an unexpected problem. Maybe the device was just disconnected?
            return false;
        }


        m_id = internal::NormalizeDeviceInstanceIdHStringCopy(pnpinfoParent->InstanceId());
        m_containerId = pnpinfoParent->ContainerId() ? pnpinfoParent->ContainerId().value() : foundation::GuidHelper::Empty();
        if (pnpinfoParent->GetDeviceUInt32(DEVPKEY_Device_ReportedDeviceIdsHash).has_value())
        {
            m_reportedDeviceIdsHash = pnpinfoParent->GetDeviceUInt32(DEVPKEY_Device_ReportedDeviceIdsHash).value();
        }

        // get properties from the media parent device, like driver name etc.
        // not every endpoint will have this
        winrt::hstring idForDriverProperties{};
        
        if (!mediaDriverDeviceParentInstanceId.empty())
        {
            // we have a provided &MI_00 device, so use it for driver properties
            idForDriverProperties = mediaDriverDeviceParentInstanceId;
            m_relatedParentDeviceMediaInstanceId = internal::NormalizeDeviceInstanceIdHStringCopy(mediaDriverDeviceParentInstanceId);
        }
        else
        {
            // no &MI_00 device, so fall back to actual parent
            idForDriverProperties = actualParentDeviceInstanceId;
            m_relatedParentDeviceMediaInstanceId = {};
        }
        
        auto driverPropertiesNode = internal::MidiPnpDeviceInfo::CreateFromInstanceId(idForDriverProperties.c_str());
        if (driverPropertiesNode)
        {
            m_driverInfPath = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_DriverInfPath);
            m_driverKey = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_Driver);
            m_driverProvider = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_DriverProvider);
            m_serviceName = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_Service);
            m_driverVersion = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_DriverVersion);
            m_enumeratorName = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_EnumeratorName);

            m_parentDeviceInstanceId = driverPropertiesNode->GetDeviceString(DEVPKEY_Device_Parent);


            if (EnumeratorName() == L"PCI")
            {
                m_name = driverPropertiesNode->Name();
            }
            else
            {
                m_name = driverPropertiesNode->NamePreferringBusDescription();
            }
        }

        return true;



        //auto pnpinfo = internal::MidiPnpDeviceInfo::CreateFromInstanceId(DeviceInstanceId());

        //if (pnpinfo)
        //{
        //    if (EnumeratorName() == L"PCI")
        //    {
        //        m_name = pnpinfo->Name();
        //    }
        //    else
        //    {
        //        m_name = pnpinfo->NamePreferringBusDescription();
        //    }
        //}
        //else
        //{
        //    m_name = deviceInformation.Name();
        //}


    }

    _Use_decl_annotations_
    midi2enum::MidiParentDeviceInformation MidiParentDeviceInformation::InternalCreateFromIds(
        winrt::hstring const& deviceInstanceId,
        winrt::hstring const& mediaDriverDeviceId) noexcept
    {

        // Fast fail: validate the devnode exists via cfgmgr before we pay
        // for the lookup. This also catches phantom devices.
        if (!internal::MidiPnpDeviceInfo::CreateFromInstanceId(deviceInstanceId.c_str()).has_value())
        {
            return nullptr;
        }

        auto parentDeviceInfo = winrt::make_self<MidiParentDeviceInformation>();
        if (parentDeviceInfo->InternalInitialize(deviceInstanceId.c_str(), mediaDriverDeviceId.c_str()))
        {
            return *parentDeviceInfo;
        }
        else
        {
            return nullptr;
        }
    }

    //_Use_decl_annotations_
    //collections::IVectorView<midi2enum::MidiParentDeviceInformation> MidiParentDeviceInformation::FindAllForContainer(winrt::guid const& containerId) noexcept
    //{
    //    // TODO
    //    UNREFERENCED_PARAMETER(containerId);

    //    return nullptr;
    //}



    //collections::IVectorView<winrt::hstring> MidiParentDeviceInformation::GetAdditionalPropertiesList() noexcept
    //{
    //    auto props = winrt::single_threaded_vector<winrt::hstring>();

    //    props.Append(L"System.Devices.ContainerId");
    //    props.Append(L"System.Devices.DeviceInstanceId");
    //    props.Append(L"System.Devices.Parent");
    //    // props.Append(L"System.Devices.Manufacturer");
    //   // props.Append(L"System.Devices.ModelName");

    //    props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_DriverInfPath));
    //    props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_Driver));
    //    props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_DriverProvider));
    //    props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_Service));
    //    props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_DriverVersion));
    //    props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_EnumeratorName));

    //    return props.GetView();
    //}

    winrt::hstring MidiParentDeviceInformation::ToString() const noexcept
    {
        winrt::hstring baseName{ L"MidiParentDeviceInformation: " };

        if (Name().empty())
        {
            // TODO: Move to resources
            baseName = baseName + L"Unnamed Parent Device";
        }
        else
        {
            baseName = baseName + Name();
        }

        return baseName;
    }
}
