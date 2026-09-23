// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Covers moving duplicate device naming out of the individual transports and into the service.
//
// Two defects are fixed by the same mechanism. The number given to the second and later units of a
// model climbed every time one was unplugged and reconnected, because each transport forgot which
// number a device held as soon as it went away and then took the highest number in use plus one.
// And two units of the same model on different transports never saw each other at all, so both
// published the same name.
//
// The service assigns the number now, from a claim recorded on the endpoint itself, so a returning
// device gets its own number back and a device from any transport is counted.

class Feature_Servicing_MIDI2DuplicateDeviceNaming
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2DuplicateDeviceNaming_IsEnabled()
{
    return true;
}
