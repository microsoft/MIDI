// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiMessageConverter.g.h"

namespace winrt::Microsoft::Devices::Midi2::Messages::implementation
{
    struct MidiMessageConverter
    {
        MidiMessageConverter() = default;

        static midi2::MidiMessage32 ConvertMidi1Message(
            _In_ internal::MidiTimestamp timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const statusByte
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1Message(
            _In_ internal::MidiTimestamp timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const statusByte,
            _In_ uint8_t const dataByte1
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1Message(
            _In_ internal::MidiTimestamp timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ uint8_t const statusByte,
            _In_ uint8_t const dataByte1,
            _In_ uint8_t const dataByte2
        ) noexcept;




        // System Common and System Real-time

        static midi2::MidiMessage32 ConvertMidi1TimeCodeMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiTimeCodeMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1SongPositionPointerMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiSongPositionPointerMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1SongSelectMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiSongSelectMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1TuneRequestMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiTuneRequestMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1TimingClockMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiTimingClockMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1StartMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiStartMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1ContinueMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiContinueMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1StopMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiStopMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1ActiveSensingMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiActiveSensingMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1SystemResetMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiSystemResetMessage const& originalMessage
        ) noexcept;


        // Channel Voice

        static midi2::MidiMessage32 ConvertMidi1ChannelPressureMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiChannelPressureMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1NoteOffMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiNoteOffMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1NoteOnMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiNoteOnMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1PitchBendChangeMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiPitchBendChangeMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1PolyphonicKeyPressureMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiPolyphonicKeyPressureMessage const& originalMessage
        ) noexcept;

        static midi2::MidiMessage32 ConvertMidi1ProgramChangeMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ midi1::MidiProgramChangeMessage const& originalMessage
        ) noexcept;

    private:
        static uint32_t InternalConvertBytes(
            _In_ uint8_t const groupIndex,
            _In_ midi1::IMidiMessage const& originalMessage
        ) noexcept;

    };
}
namespace winrt::Microsoft::Devices::Midi2::Messages::factory_implementation
{
    struct MidiMessageConverter : MidiMessageConverterT<MidiMessageConverter, implementation::MidiMessageConverter, winrt::static_lifetime>
    {
    };
}
