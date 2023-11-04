// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageTranslator.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageTranslator
    {
        MidiMessageTranslator() = default;

        static midi2::MidiMessage64 UpscaleMidi1ChannelVoiceMessageToMidi2(
            _In_ midi2::MidiMessage32 const& originalMessage
        ) noexcept;

        static midi2::MidiMessage64 UpscaleMidi1ChannelVoiceMessageToMidi2(
            _In_ internal::MidiTimestamp timestamp,
            _In_ uint8_t groupIndex,
            _In_ uint8_t statusByte
        ) noexcept;

        static midi2::MidiMessage64 UpscaleMidi1ChannelVoiceMessageToMidi2(
            _In_ internal::MidiTimestamp timestamp,
            _In_ uint8_t groupIndex,
            _In_ uint8_t statusByte,
            _In_ uint8_t dataByte1
        ) noexcept;

        static midi2::MidiMessage64 UpscaleMidi1ChannelVoiceMessageToMidi2(
            _In_ internal::MidiTimestamp timestamp,
            _In_ uint8_t groupIndex,
            _In_ uint8_t statusByte,
            _In_ uint8_t dataByte1,
            _In_ uint8_t dataByte2
        ) noexcept;

        static midi2::MidiMessage32 DownscaleMidi2ChannelVoiceMessageToMidi1(
            _In_ midi2::MidiMessage64 const& originalMessage
        ) noexcept;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageTranslator : MidiMessageTranslatorT<MidiMessageTranslator, implementation::MidiMessageTranslator>
    {
    };
}
