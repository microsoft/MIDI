// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "swd_shared.h"

namespace Windows::Devices::Midi2::Internal
{

    std::wstring
    GetStringSwdProperty(_In_ std::wstring deviceInterfaceId, _In_ std::wstring propertyName, _In_ std::wstring defaultValue)
    {
        auto propertyKey = winrt::to_hstring(propertyName.c_str());

        auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
        additionalProperties.Append(propertyKey);


        auto deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
            winrt::to_hstring(deviceInterfaceId.c_str()),
            additionalProperties,
            winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

        auto prop = deviceInfo.Properties().Lookup(propertyKey);

        if (prop)
        {
            OutputDebugString(__FUNCTION__ L" found property");

            // this interface is pointing to a UMP interface, so use that instance id.
            return (winrt::unbox_value<winrt::hstring>(prop)).c_str();
        }
        else
        {
            OutputDebugString(__FUNCTION__ L" didn't find property");
            // default to any
            return defaultValue;
        }

    }


    _Use_decl_annotations_
    std::wstring
    GetSwdPropertyVirtualEndpointAssociationId(std::wstring deviceInterfaceId)
    {
        std::wstring cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

        return internal::ToUpperTrimmedWStringCopy(GetStringSwdProperty(cleanId, STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L""));
    }


    _Use_decl_annotations_
    std::wstring
    GetSwdPropertyInstanceId(std::wstring deviceInterfaceId)
    {
        std::wstring cleanId = internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

        return internal::NormalizeDeviceInstanceIdWStringCopy(GetStringSwdProperty(cleanId, L"System.Devices.DeviceInstanceId", L""));
    }
}