// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiGroup.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup>
    {
        //MidiGroup() = default;
        MidiGroup(_In_ uint8_t index) noexcept { Index(index); }


        uint8_t Index() const noexcept { return m_index; }
        void Index(_In_ uint8_t value) noexcept { if (MidiGroup::IsValidGroupIndex(value)) m_index = value; }

        static winrt::hstring ShortLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_SHORT); }
        static winrt::hstring LongLabel() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_GROUP_FULL); }


        uint8_t NumberForDisplay() const noexcept { return m_index + 1; }


        static bool IsValidGroupIndex(_In_ uint8_t const index) noexcept { return index >= 0 && index <= 15; }

    private:
        uint8_t m_index{ 0 };
    };
}

namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiGroup : MidiGroupT<MidiGroup, implementation::MidiGroup>
    {
    };
}
