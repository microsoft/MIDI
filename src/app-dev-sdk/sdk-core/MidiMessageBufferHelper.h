// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageBufferHelper.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessageBufferHelper
    {
        MidiMessageBufferHelper() = default;

        static winrt::Windows::Foundation::IMemoryBuffer CreateMemoryBuffer(uint32_t midiWordCount);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiMessageBufferHelper : MidiMessageBufferHelperT<MidiMessageBufferHelper, implementation::MidiMessageBufferHelper>
    {
    };
}
