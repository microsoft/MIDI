// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef SWD_HELPERS_H
#define SWD_HELPERS_H


#include <string>



namespace Windows::Devices::Midi2::Internal
{
    inline std::wstring CalculateEndpointDevicePrimaryName(
        _In_ std::wstring const& transportSuppliedEndpointName,
        _In_ std::wstring const& userSuppliedEndpointName,
        _In_ std::wstring const& inProtocolDiscoveredEndpointName
    )
    {
        // top priority is any user-supplied name
        if (!::Windows::Devices::Midi2::Internal::TrimmedWStringCopy(userSuppliedEndpointName).empty())
        {
            return ::Windows::Devices::Midi2::Internal::TrimmedWStringCopy(userSuppliedEndpointName);
        }

        // next priority is any in-protocol name
        if (!internal::TrimmedWStringCopy(inProtocolDiscoveredEndpointName).empty())
        {
            return ::Windows::Devices::Midi2::Internal::TrimmedWStringCopy(inProtocolDiscoveredEndpointName);
        }

        // otherwise, we return the transport-supplied name. For example, this is the name from USB

        return ::Windows::Devices::Midi2::Internal::TrimmedWStringCopy(transportSuppliedEndpointName);

    }

    // This is for the device instance id. Not to be confused with the interface id
    inline std::wstring NormalizeDeviceInstanceIdWStringCopy(_In_ std::wstring const& deviceInstanceId)
    {
        return ::Windows::Devices::Midi2::Internal::ToUpperTrimmedWStringCopy(deviceInstanceId);
    }

    // This is for the endpoint device interface id (the long SWD id with the GUID)
    inline std::wstring NormalizeEndpointInterfaceIdWStringCopy(_In_ std::wstring const& endpointInterfaceId)
    {
         return ::Windows::Devices::Midi2::Internal::ToLowerTrimmedWStringCopy(endpointInterfaceId);
    }

    // used for searching for a substring in an endpoint interface id. Matches case with
    // what NormalizeEndpointInterfaceIdCopy produces
    inline bool EndpointInterfaceIdContainsString(_In_ std::wstring const& endpointInterfaceId, _In_ std::wstring const& searchFor)
    {
        auto id = NormalizeEndpointInterfaceIdWStringCopy(endpointInterfaceId);
        auto sub = NormalizeEndpointInterfaceIdWStringCopy(searchFor);             // match case with NormalizeEndpointInterfaceIdCopy

        if (id.empty() || sub.empty())
        {
            return false;
        }

        return id.find(sub) != std::wstring::npos;        
    }



    inline DEVPROPERTY BuildEmptyDevProperty(
        _In_ DEVPROPKEY const key)
    {
        return DEVPROPERTY{ {key, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_EMPTY, 0, nullptr };        
    }


    inline std::wstring GetSwdStringProperty(_In_ std::wstring deviceInterfaceId, _In_ std::wstring propertyName, _In_ std::wstring defaultValue)
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
            // this interface is pointing to a UMP interface, so use that instance id.
            return (winrt::unbox_value<winrt::hstring>(prop)).c_str();
        }
        else
        {
            // default to any
            return defaultValue;
        }
    }

    inline std::wstring GetSwdPropertyVirtualEndpointAssociationId(_In_ std::wstring deviceInterfaceId)
    {
        std::wstring cleanId = ::Windows::Devices::Midi2::Internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

        return internal::ToUpperTrimmedWStringCopy(GetSwdStringProperty(cleanId, STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L""));
    }

    inline std::wstring GetSwdPropertyInstanceId(_In_ std::wstring deviceInterfaceId)
    {
        std::wstring cleanId = ::Windows::Devices::Midi2::Internal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

        return internal::NormalizeDeviceInstanceIdWStringCopy(GetSwdStringProperty(cleanId, L"System.Devices.DeviceInstanceId", L""));
    }

}

#endif
