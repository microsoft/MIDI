// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef SWD_PROPERTY_HELPERS_H
#define SWD_PROPERTY_HELPERS_H

#include "MidiDefs.h"
#include "MidiDataFormat.h"

namespace Windows::Devices::Midi2::Internal
{
    bool AddStandardEndpointProperties(
        _In_ std::vector<DEVPROPERTY>& interfaceDeviceProperties,

        _In_ GUID const& abstractionLayerGuid,
        _In_ MidiEndpointDevicePurposePropertyValue const& endpointPurpose,

        _In_ std::wstring const& friendlyName,

        _In_ std::wstring const& transportMnemonic,
        _In_ std::wstring const& transportSuppliedEndpointName,
        _In_ std::wstring const& transportSuppliedEndpointDescription,

        _In_ std::wstring const& userSuppliedEndpointName,
        _In_ std::wstring const& userSuppliedEndpointDescription,

        _In_ std::wstring const& uniqueIdentifier,

        _In_ MidiDataFormat const& supportedDataFormats,
        _In_ BYTE const& nativeDataFormat,

        _In_ bool const& supportsMultiClient,
        _In_ bool const& requiresMetadataHandler,
        _In_ bool const& generateIncomingTimestamps
    );


}

#endif