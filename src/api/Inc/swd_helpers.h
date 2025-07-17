// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef SWD_HELPERS_H
#define SWD_HELPERS_H

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <string>



namespace WindowsMidiServicesInternal
{
    inline std::wstring CalculateEndpointDevicePrimaryName(
        _In_ std::wstring const& transportSuppliedEndpointName,
        _In_ std::wstring const& customEndpointName,
        _In_ std::wstring const& inProtocolDiscoveredEndpointName
    )
    {
        // top priority is any user-supplied name
        if (!::WindowsMidiServicesInternal::TrimmedWStringCopy(customEndpointName).empty())
        {
            return ::WindowsMidiServicesInternal::TrimmedWStringCopy(customEndpointName);
        }

        // next priority is any in-protocol name
        if (!::WindowsMidiServicesInternal::TrimmedWStringCopy(inProtocolDiscoveredEndpointName).empty())
        {
            return ::WindowsMidiServicesInternal::TrimmedWStringCopy(inProtocolDiscoveredEndpointName);
        }

        // otherwise, we return the transport-supplied name. For example, this is the name from USB

        return ::WindowsMidiServicesInternal::TrimmedWStringCopy(transportSuppliedEndpointName);

    }

    // This is for the device instance id. Not to be confused with the interface id
    inline std::wstring NormalizeDeviceInstanceIdWStringCopy(_In_ std::wstring const& deviceInstanceId)
    {
        return ::WindowsMidiServicesInternal::ToUpperTrimmedWStringCopy(deviceInstanceId);
    }

    // This is for the endpoint device interface id (the long SWD id with the GUID)
    inline std::wstring NormalizeEndpointInterfaceIdWStringCopy(_In_ std::wstring const& endpointInterfaceId)
    {
         return ::WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(endpointInterfaceId);
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
        std::wstring cleanId = ::WindowsMidiServicesInternal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

        return ::WindowsMidiServicesInternal::ToUpperTrimmedWStringCopy(GetSwdStringProperty(cleanId, STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L""));
    }

    inline std::wstring GetSwdPropertyInstanceId(_In_ std::wstring deviceInterfaceId)
    {
        std::wstring cleanId = ::WindowsMidiServicesInternal::NormalizeEndpointInterfaceIdWStringCopy(deviceInterfaceId);

        return ::WindowsMidiServicesInternal::NormalizeDeviceInstanceIdWStringCopy(GetSwdStringProperty(cleanId, L"System.Devices.DeviceInstanceId", L""));
    }

    // WinRT doesn't allow us pass the property bag around separately, so have to pass the whole deviceinformation object in here
    template <class T>
    inline T SafeGetSwdPropertyFromDeviceInformation(
        _In_ winrt::hstring propertyStringKey, 
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformation deviceInformation,
        _In_ T defaultValue)
    {
        if (propertyStringKey.empty())
        {
            return defaultValue;
        }

        if (deviceInformation == nullptr || !deviceInformation.Properties().HasKey(propertyStringKey))
        {
            return defaultValue;
        }

        // lookup the property. It should provide us with an IInspectable
        auto prop = deviceInformation.Properties().Lookup(propertyStringKey);

        if (prop != nullptr)
        {
            return winrt::unbox_value_or<T>(prop, defaultValue);
        }
        else
        {
            return defaultValue;
        }
    }


    inline winrt::Windows::Foundation::IReferenceArray<uint8_t> SafeGetSwdBinaryPropertyFromDeviceInformation(
        _In_ winrt::hstring propertyStringKey,
        _In_ winrt::Windows::Devices::Enumeration::DeviceInformation deviceInformation)
    {
        if (propertyStringKey.empty())
        {
            return nullptr;
        }

        if (deviceInformation == nullptr || !deviceInformation.Properties().HasKey(propertyStringKey))
        {
            return nullptr;
        }

        // lookup the property. It should provide us with an IInspectable
        auto prop = deviceInformation.Properties().Lookup(propertyStringKey);

        if (prop != nullptr)
        {
            return winrt::unbox_value_or<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(prop, nullptr);
        }
        else
        {
            return nullptr;
        }
    }


    inline bool ReferenceByteArraysAreIdentical(
        _In_ winrt::Windows::Foundation::IReferenceArray<uint8_t> refA,
        _In_ winrt::Windows::Foundation::IReferenceArray<uint8_t> refB)
    {
        if (refA == nullptr && refB == nullptr)
        {
            return true;
        }

        if ((refA == nullptr && refB != nullptr) ||
            (refA != nullptr && refB == nullptr))
        {
            return false;
        }

        // nothing is null, so continue processing
        
        auto refAData = refA.Value();
        auto refBData = refB.Value();

        if (refAData.size() != refBData.size())
        {
            // different size arrays
            return false;
        }

        // at this point, we have same size arrays, so just compare
        auto cmp = memcmp(refAData.data(), refBData.data(), refAData.size());

        return (bool)(cmp == 0);
    }

    inline bool ReferenceByteArrayAndByteVectorAreIdentical(
        _In_ winrt::Windows::Foundation::IReferenceArray<uint8_t> refA,
        _In_ std::vector<std::byte> vect)
    {
        if (refA == nullptr && vect.size() == 0)
        {
            return true;
        }

        if (refA == nullptr && vect.size() != 0)
        {
            return false;
        }

        // nothing is null, so continue processing

        auto refAData = refA.Value();

        if (refAData.size() != vect.size())
        {
            // different size arrays
            return false;
        }

        // at this point, we have same size data, so just compare. Both are contiguous in memory
        auto cmp = memcmp(refAData.data(), vect.data(), refAData.size());

        return (bool)(cmp == 0);
    }


}

#endif
