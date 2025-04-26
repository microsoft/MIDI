// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiUniversalSystemExclusiveChannel.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiUniversalSystemExclusiveChannel : MidiUniversalSystemExclusiveChannelT<MidiUniversalSystemExclusiveChannel>
    {
        static winrt::hstring ShortLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_SYSEX_CHANNEL_SHORT); }
        static winrt::hstring ShortLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_SYSEX_CHANNEL_SHORT_PLURAL); }
        static winrt::hstring LongLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_SYSEX_CHANNEL_FULL); }
        static winrt::hstring LongLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_SYSEX_CHANNEL_FULL_PLURAL); }

        static midi2::MidiUniversalSystemExclusiveChannel DisregardChannel() { return midi2::MidiUniversalSystemExclusiveChannel(127); }

        MidiUniversalSystemExclusiveChannel() noexcept { Index(0); }
        MidiUniversalSystemExclusiveChannel(_In_ uint8_t index) noexcept { Index(index); }

        uint8_t Index() const noexcept { return m_index; }
        void Index(_In_ uint8_t value) noexcept { m_index = (value & 0x7F); }

        uint8_t DisplayValue() const noexcept { return m_index + 1; }

        static bool IsValidIndex(_In_ uint8_t const index) noexcept { return index >= 0 && index <= 127; }

        // IStringable
        winrt::hstring ToString();

    private:
        uint8_t m_index{ 0 };

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUniversalSystemExclusiveChannel : MidiUniversalSystemExclusiveChannelT<MidiUniversalSystemExclusiveChannel, implementation::MidiUniversalSystemExclusiveChannel>
    {
    };
}
