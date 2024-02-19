// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageConverter.g.h"

namespace winrt::Windows::Devices::Midi2::implementation
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
            _In_ winrt::Windows::Devices::Midi::MidiTimeCodeMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return nullptr;
        }

        static midi2::MidiMessage32 ConvertMidi1SongPositionPointerMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiSongPositionPointerMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1SongSelectMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiSongSelectMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1TuneRequestMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiTuneRequestMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return nullptr;
        }


        static midi2::MidiMessage32 ConvertMidi1TimingClockMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiTimingClockMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return nullptr;
        }


        static midi2::MidiMessage32 ConvertMidi1StartMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiStartMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1ContinueMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiContinueMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return message;
        }


        static midi2::MidiMessage32 ConvertMidi1StopMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiStopMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1ActiveSensingMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiActiveSensingMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return nullptr;
        }

        static midi2::MidiMessage32 ConvertMidi1SystemResetMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiSystemResetMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::SystemCommon32);

            return nullptr;
        }


        // Channel Voice

        static midi2::MidiMessage32 ConvertMidi1ChannelPressureMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiChannelPressureMessage const& originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

            return message;
        }


        static midi2::MidiMessage32 ConvertMidi1NoteOffMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiNoteOffMessage const&  originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

            return message;
        }


        static midi2::MidiMessage32 ConvertMidi1NoteOnMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiNoteOnMessage const&  originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1PitchBendChangeMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiPitchBendChangeMessage const&  originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1PolyphonicKeyPressureMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiPolyphonicKeyPressureMessage const&  originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

            return message;
        }

        static midi2::MidiMessage32 ConvertMidi1ProgramChangeMessage(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ midi2::MidiGroup const& group,
            _In_ winrt::Windows::Devices::Midi::MidiProgramChangeMessage const&  originalMessage
        ) noexcept
        {
            auto message = winrt::make<implementation::MidiMessage32>();
            message.Timestamp(timestamp);
            message.Word0(InternalConvertBytes(group.Index(), (Windows::Devices::Midi::IMidiMessage)originalMessage));

            message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

            return message;
        }





    private:
        static uint32_t InternalConvertBytes(
            _In_ uint8_t const groupIndex,
            _In_ Windows::Devices::Midi::IMidiMessage const& originalMessage
        ) noexcept;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageConverter : MidiMessageConverterT<MidiMessageConverter, implementation::MidiMessageConverter, winrt::static_lifetime>
    {
    };
}
