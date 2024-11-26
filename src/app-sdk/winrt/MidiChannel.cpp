// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiChannel.h"
#include "MidiChannel.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    winrt::hstring MidiChannel::ToString()
    {
        return winrt::to_hstring(std::format("{} {}", winrt::to_string(midi2::MidiChannel::ShortLabel()), DisplayValue()));
    }

}
