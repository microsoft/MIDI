// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageBufferHelper.h"
#include "MidiMessageBufferHelper.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Windows::Foundation::IMemoryBuffer MidiMessageBufferHelper::CreateMemoryBuffer(uint32_t midiWordCount)
    {
        throw hresult_not_implemented();
    }
}
