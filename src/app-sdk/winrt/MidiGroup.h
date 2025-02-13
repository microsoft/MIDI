// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#pragma once
#include "MidiGroup.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup>
    {
        static winrt::hstring ShortLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_SHORT); }
        static winrt::hstring ShortLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_SHORT_PLURAL); }
        static winrt::hstring LongLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_FULL); }
        static winrt::hstring LongLabelPlural() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_FULL_PLURAL); }

        MidiGroup() noexcept { Index(0); }
        MidiGroup(_In_ uint8_t index) noexcept { Index(index); }

        uint8_t Index() const noexcept { return m_index; }
        void Index(_In_ uint8_t value) noexcept { m_index = (value & 0x0F); }

        uint8_t DisplayValue() const noexcept { return m_index + 1; }

        // IStringable
        winrt::hstring ToString();

        static bool IsValidIndex(_In_ uint8_t const index) noexcept { return index >= 0 && index <= 15; }

    private:
        uint8_t m_index{ 0 };
    };
}

namespace winrt::Microsoft::Windows::Devices::Midi2::factory_implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup, implementation::MidiGroup>
    {
    };
}
