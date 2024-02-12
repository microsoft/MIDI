// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef SWD_PROPERTY_BUILDERS_H
#define SWD_PROPERTY_BUILDERS_H

#include "Devpkey.h"
#include <string>


namespace Windows::Devices::Midi2::Internal
{
    DEVPROPERTY BuildWStringDevProperty(
        _In_ DEVPROPKEY const key,
        _In_ std::wstring const& value);

    DEVPROPERTY BuildUInt32DevProperty(
        _In_ DEVPROPKEY const key,
        _In_ uint32_t const& value);

    DEVPROPERTY BuildByteDevProperty(
        _In_ DEVPROPKEY const key,
        _In_ uint8_t const& value);

    DEVPROPERTY BuildGuidDevProperty(
        _In_ DEVPROPKEY const key,
        _In_ GUID const& value);

    DEVPROPERTY BuildBooleanDevProperty(
        _In_ DEVPROPKEY const key,
        _In_ bool const& value);

    DEVPROPERTY BuildEmptyDevProperty(
        _In_ DEVPROPKEY const key);

}

#endif