// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageBuffer.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageBuffer : MidiMessageBufferT<MidiMessageBuffer>
    {
        MidiMessageBuffer() = default;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageBuffer : MidiMessageBufferT<MidiMessageBuffer, implementation::MidiMessageBuffer>
    {
    };
}
