// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Some legacy Yamaha USB MIDI devices (YAMAHA UX96, YAMAHA MU1000) declare class 0xFF, but
// their packet format is USB MIDI 1.0. With this enabled, for a VID/PID in the quirk table the
// USBMIDI2 driver accepts that interface as USB MIDI 1.0, counts IN/OUT jacks of any type,
// reverses their direction and lets writes to the output cables pass the input cable check.
// With it disabled the shipped code path runs (the INF hardware-ID match cannot be rolled back).

class Feature_Servicing_MIDI2USBYamahaVendorClassMidi
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2USBYamahaVendorClassMidi_IsEnabled()
{
    return true;
}
