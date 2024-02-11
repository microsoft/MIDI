// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef SWD_DATA_HELPERS_H
#define SWD_DATA_HELPERS_H


namespace Windows::Devices::Midi2::Internal
{
    std::wstring CalculateEndpointDevicePrimaryName(
        _In_ std::wstring const& transportSuppliedEndpointName,
        _In_ std::wstring const& userSuppliedEndpointName,
        _In_ std::wstring const& inProtocolDiscoveredEndpointName
    );

    // This is for the device instance id. Not to be confused with the interface id
    std::wstring NormalizeDeviceInstanceIdWStringCopy(_In_ std::wstring const& deviceInstanceId);

    // This is for the endpoint device interface id (the long SWD id with the GUID)
    std::wstring NormalizeEndpointInterfaceIdWStringCopy(_In_ std::wstring const& endpointInterfaceId);

    // used for searching for a substring in an endpoint interface id. Matches case with
    // what NormalizeEndpointInterfaceIdCopy produces
    bool EndpointInterfaceIdContainsString(_In_ std::wstring const& endpointInterfaceId, _In_ std::wstring const& searchFor);

}

#endif
