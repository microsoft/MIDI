// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiMessageConverter.h"
#include "Messages.MidiMessageConverter.g.cpp"

#include "midi1_message_defs.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Messages::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1Message(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const statusByte
    ) noexcept
    {
        return ConvertMidi1Message(timestamp, group, statusByte, 0, 0);
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1Message(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const statusByte,
        uint8_t const dataByte1
    ) noexcept
    {
        return ConvertMidi1Message(timestamp, group, statusByte, dataByte1, 0);
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1Message(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const statusByte,
        uint8_t const dataByte1,
        uint8_t const dataByte2
    ) noexcept
    {
        // we don't handle sysex here
        if (MIDI_BYTE_IS_SYSEX_START_STATUS(statusByte) || MIDI_BYTE_IS_SYSEX_END_STATUS(statusByte))
        {
            return nullptr;
        }


        uint32_t midiWord{ 0 };
        midiWord = internal::MidiWordFromBytes(
            (uint8_t)0x00,
            statusByte,
            internal::CleanupByte7(dataByte1),
            internal::CleanupByte7(dataByte2)
        );

        if (MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(statusByte))
        {
            // convert rt message
            internal::SetUmpMessageType(midiWord, (uint8_t)midi2::MidiMessageType::SystemCommon32);
        }
        else if (MIDI_STATUS_IS_CHANNEL_VOICE_MESSAGE(statusByte))
        {
            // convert cv message
            internal::SetUmpMessageType(midiWord, (uint8_t)midi2::MidiMessageType::Midi1ChannelVoice32);
        }

        // set the group
        internal::SetGroupIndexInFirstWord(midiWord, group.Index());

        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(midiWord);

        return message;
    }



    _Use_decl_annotations_
    uint32_t MidiMessageConverter::InternalConvertBytes(
        uint8_t const groupIndex,
        midi1::IMidiMessage const& originalMessage
    ) noexcept
    {
        // get the bytes using IBufferByteAccess and then do the conversion

        auto dataPointer = originalMessage.RawData().data();
        auto numBytes = originalMessage.RawData().Length();

        uint32_t midiWord{ 0 };

        if (numBytes == 3)
        {
            midiWord = internal::MidiWordFromBytes(
                (uint8_t)0x00,  // message type and group
                dataPointer[0], // status
                internal::CleanupByte7(dataPointer[1]),
                internal::CleanupByte7(dataPointer[2])
            );

        }
        else if (numBytes == 2)
        {
            midiWord = internal::MidiWordFromBytes(
                (uint8_t)0x00,  // message type and group
                dataPointer[0], // status
                internal::CleanupByte7(dataPointer[1]),
                (uint8_t)0x00
            );

        }
        else if (numBytes == 1)
        {
            midiWord = internal::MidiWordFromBytes(
                (uint8_t)0x00,  // message type and group
                dataPointer[0], // status
                (uint8_t)0x00,
                (uint8_t)0x00
            );
        }

        // TODO: this is an assumption. We really should check the message type
        internal::SetUmpMessageType(midiWord, (uint8_t)midi2::MidiMessageType::Midi1ChannelVoice32);
        //if (originalMessage.Type() == Windows::Devices::Midi::MidiMessageType::NoteOff) ...

        // set the group
        internal::SetGroupIndexInFirstWord(midiWord, groupIndex);

        return midiWord;
    }




    // System Common and System Real-time

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1TimeCodeMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiTimeCodeMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1SongPositionPointerMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiSongPositionPointerMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1SongSelectMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiSongSelectMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1TuneRequestMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiTuneRequestMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1TimingClockMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiTimingClockMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1StartMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiStartMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1ContinueMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiContinueMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }


    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1StopMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiStopMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1ActiveSensingMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiActiveSensingMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1SystemResetMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiSystemResetMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::SystemCommon32);

        return message;
    }


    // Channel Voice

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1ChannelPressureMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiChannelPressureMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

        return message;
    }


    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1NoteOffMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiNoteOffMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

        return message;
    }


    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1NoteOnMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiNoteOnMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1PitchBendChangeMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiPitchBendChangeMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1PolyphonicKeyPressureMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiPolyphonicKeyPressureMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

        return message;
    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1ProgramChangeMessage(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        midi1::MidiProgramChangeMessage const& originalMessage
    ) noexcept
    {
        midi2::MidiMessage32 message;
        message.Timestamp(timestamp);
        message.Word0(InternalConvertBytes(group.Index(), (midi1::IMidiMessage)originalMessage));

        message.MessageType(midi2::MidiMessageType::Midi1ChannelVoice32);

        return message;
    }



    _Use_decl_annotations_
    collections::IVector<uint32_t> MidiMessageConverter::ConvertMidi1CompleteMessageBytesToUmpWords(
        midi2::MidiGroup const& group,
        collections::IIterable<uint8_t> const& midi1Bytes,
        bool const allowRunningStatus
    ) noexcept
    {
        bytestreamToUMP converter;

        auto words = winrt::single_threaded_vector<uint32_t>();

        converter.defaultGroup = group.Index();
        converter.enableRunningStatus = allowRunningStatus;

        auto it = midi1Bytes.First();
        while (it.HasCurrent())
        {
            converter.bytestreamParse(it.Current());

            while (converter.availableUMP())
            {
                words.Append(converter.readUMP());
            }

            it.MoveNext();
        }

        return words;
    }

    _Use_decl_annotations_
    collections::IVector<uint32_t> MidiMessageConverter::ConvertMidi1MessageToUmpWords(
        _In_ midi2::MidiGroup const& group,
        _In_ midi1::IMidiMessage const& originalMessage
    ) noexcept
    {
        bytestreamToUMP converter;

        auto words = winrt::single_threaded_vector<uint32_t>();

        converter.defaultGroup = group.Index();
        converter.enableRunningStatus = false;

        auto data = originalMessage.RawData().data();
        auto dataLength = originalMessage.RawData().Length();

        for (uint32_t i = 0; i < dataLength; i++)
        {
            converter.bytestreamParse(data[i]);

            // if on the last byte, ensure we close out sysex
            // in case this message isn't complete SysEx with F7
            if (i == dataLength -1)
            {
                converter.dumpSysex7State(true);
            }

            while (converter.availableUMP())
            {
                words.Append(converter.readUMP());
            }
        }

        return words;

    }



    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiMessageConverter::ConvertSingleGroupCompleteMessageUmpWordsToMidi1Bytes(
        _In_ collections::IIterable<uint32_t> const& umpWords
    ) noexcept
    {
        umpToBytestream converter;

        auto bytes = winrt::single_threaded_vector<uint8_t>();

        auto it = umpWords.First();
        while (it.HasCurrent())
        {
            converter.UMPStreamParse(it.Current());

            while (converter.availableBS())
            {
                bytes.Append(converter.readBS());
            }

            it.MoveNext();
        }

        return bytes;
    }



    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiMessageConverter::ConvertHexByteStringToByteArray(
        _In_ winrt::hstring const& hexByteString
    ) noexcept
    {
        try
        {
            auto bytes = winrt::single_threaded_vector<uint8_t>();

            std::wstring s { hexByteString };

            std::wstring currentByte{};

            auto it = s.begin();

            while (it != s.end())
            {
                if (std::isxdigit(*it))
                {
                    currentByte.push_back(*it);
                }
                else if (!currentByte.empty())
                {
                    // not a hex digit, so treat as a delimiter. 
                    // we already have some data started, so 
                    // commit it

                    bytes.Append(static_cast<uint8_t>(std::stoi(currentByte, nullptr, 16)));
                    currentByte.clear();
                }

                if (currentByte.size() == 2)
                {
                    bytes.Append(static_cast<uint8_t>(std::stoi(currentByte, nullptr, 16)));
                    currentByte.clear();
                }

                it++;
            }

            // ensure the last digit is captured
            if (!currentByte.empty())
            {
                bytes.Append(static_cast<uint8_t>(std::stoi(currentByte, nullptr, 16)));
            }

            return bytes;
        }
        catch (...)
        {
            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception attempting to convert string to bytes", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }

    }


}
