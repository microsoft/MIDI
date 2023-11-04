// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageTranslator.h"
#include "MidiMessageTranslator.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(
        midi2::MidiMessage32 const& /*originalMessage*/
    ) noexcept
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(
        internal::MidiTimestamp /*timestamp*/,
        uint8_t /*groupIndex*/,
        uint8_t /*statusByte*/
    ) noexcept
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(
        internal::MidiTimestamp /*timestamp*/,
        uint8_t /*groupIndex*/,
        uint8_t /*statusByte*/,
        uint8_t /*dataByte1*/
    ) noexcept
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessageTranslator::UpscaleMidi1ChannelVoiceMessageToMidi2(
        internal::MidiTimestamp /*timestamp*/,
        uint8_t /*groupIndex*/,
        uint8_t /*statusByte*/,
        uint8_t /*dataByte1*/,
        uint8_t /*dataByte2*/
    ) noexcept
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageTranslator::DownscaleMidi2ChannelVoiceMessageToMidi1(
        midi2::MidiMessage64 const& /*originalMessage*/
        ) noexcept
    {
        // TODO
        return nullptr;
    }
}
