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
    _Use_decl_annotations_
    midi2enum::MidiParentDeviceInformation MidiParentDeviceInformation::CreateFromId(
        winrt::hstring const& deviceInstanceId) noexcept
    {
        enumeration::DeviceInformation device{ nullptr };

        try
        {
            device = enumeration::DeviceInformation::CreateFromIdAsync(
                deviceInstanceId,
                GetAdditionalPropertiesList(),
                enumeration::DeviceInformationKind::Device
            ).get();
        }
        catch (winrt::hresult_error const&)
        {
            return nullptr;
        }

        if (!device)
        {
            return nullptr;
        }

        auto parentDeviceInfo = winrt::make_self<MidiParentDeviceInformation>();

        parentDeviceInfo->InternalInitialize(device);

        return *parentDeviceInfo;
    }

    _Use_decl_annotations_
    collections::IVectorView<midi2enum::MidiParentDeviceInformation> MidiParentDeviceInformation::FindAllForContainer(winrt::guid const& containerId) noexcept
    {
        // TODO
        UNREFERENCED_PARAMETER(containerId);

        return nullptr;
    }



    collections::IVectorView<winrt::hstring> MidiParentDeviceInformation::GetAdditionalPropertiesList() noexcept
    {
        auto props = winrt::single_threaded_vector<winrt::hstring>();

        props.Append(L"System.Devices.ContainerId");
        props.Append(L"System.Devices.DeviceInstanceId");
       // props.Append(L"System.Devices.Manufacturer");
       // props.Append(L"System.Devices.ModelName");

        props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_DriverInfPath));
        props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_Driver));
        props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_DriverProvider));
        props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_Service));
        props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_DriverVersion));
        props.Append(internal::DevPropKeyToWinRTPropertyHString(DEVPKEY_Device_EnumeratorName));

        return props.GetView();
    }

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
