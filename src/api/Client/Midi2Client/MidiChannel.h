// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiChannel.g.h"

namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiChannel : MidiChannelT<MidiChannel>
    {
        MidiChannel() = default;
        MidiChannel(_In_ uint8_t index) noexcept { Index(index); }

        uint8_t Index() const noexcept { return m_index; }
        void Index(_In_ uint8_t value) noexcept { if (MidiChannel::IsValidChannelIndex(value)) m_index = value; }
        
        static winrt::hstring LabelShort() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_CHANNEL_SHORT); }
        static winrt::hstring LabelFull() { return internal::ResourceGetHString(IDS_MIDI_COMMON_LABEL_CHANNEL_FULL); }

        uint8_t NumberForDisplay() const noexcept { return m_index + 1; }

        static bool IsValidChannelIndex(_In_ uint8_t const index) noexcept { return index >= 0 && index <= 15; }


    private:
        uint8_t m_index{ 0 };

    };
}
namespace MIDI_CPP_NAMESPACE::factory_implementation
{
    struct MidiChannel : MidiChannelT<MidiChannel, implementation::MidiChannel>
    {
    };
}
