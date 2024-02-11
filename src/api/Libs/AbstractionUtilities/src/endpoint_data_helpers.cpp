// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

namespace Windows::Devices::Midi2::Internal
{
    _Use_decl_annotations_
    std::wstring CalculateEndpointDevicePrimaryName(
        std::wstring const& transportSuppliedEndpointName,
        std::wstring const& userSuppliedEndpointName,
        std::wstring const& inProtocolDiscoveredEndpointName
    )
    {
        // top priority is any user-supplied name
        if (!internal::TrimmedWStringCopy(userSuppliedEndpointName).empty())
        {
            return internal::TrimmedWStringCopy(userSuppliedEndpointName);
        }

        // next priority is any in-protocol name
        if (!internal::TrimmedWStringCopy(inProtocolDiscoveredEndpointName).empty())
        {
            return internal::TrimmedWStringCopy(inProtocolDiscoveredEndpointName);
        }

        // otherwise, we return the transport-supplied name. For example, this is the name from USB

        return internal::TrimmedWStringCopy(transportSuppliedEndpointName);

    }

    // This is for the device instance id. Not to be confused with the interface id
    _Use_decl_annotations_
    std::wstring NormalizeDeviceInstanceIdWStringCopy(std::wstring const& deviceInstanceId)
    {
        return ToUpperTrimmedWStringCopy(deviceInstanceId);
    }


    // This is for the endpoint device interface id (the long SWD id with the GUID)
    _Use_decl_annotations_
    std::wstring NormalizeEndpointInterfaceIdWStringCopy(std::wstring const& endpointInterfaceId)
    {
        return ToLowerTrimmedWStringCopy(endpointInterfaceId);
    }

    // used for searching for a substring in an endpoint interface id. Matches case with
    // what NormalizeEndpointInterfaceIdCopy produces
    _Use_decl_annotations_
    bool EndpointInterfaceIdContainsString(std::wstring const& endpointInterfaceId, std::wstring const& searchFor)
    {
        auto id = NormalizeEndpointInterfaceIdWStringCopy(endpointInterfaceId);
        auto sub = ToLowerWStringCopy(searchFor);             // match case with NormalizeEndpointInterfaceIdCopy

        if (id == L"" || sub == L"")
        {
            return false;
        }

        return id.find(sub) != std::wstring::npos;
    }
}

