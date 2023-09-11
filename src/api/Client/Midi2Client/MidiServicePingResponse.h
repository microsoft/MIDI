// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServicePingResponse.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiServicePingResponse : MidiServicePingResponseT<MidiServicePingResponse>
    {
        MidiServicePingResponse() = default;

        uint64_t Id();
        uint64_t SendMidiTimestamp();
        uint64_t ReceiveMidiTimestamp();
        uint64_t DeltaTimestamp();
    };
}
