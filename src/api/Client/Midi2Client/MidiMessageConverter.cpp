// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageConverter.h"
#include "MidiMessageConverter.g.cpp"

namespace MIDI_CPP_NAMESPACE::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1Message(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const statusByte
    ) noexcept
    {
        uint32_t midiWord{ 0 };

        midiWord = internal::MidiWordFromBytes(
            (uint8_t)0x00,
            statusByte,
            0x00,
            0x00
        );


 //       auto statusNibble = (statusByte & 0xF0) >> 4;




        // TODO: this is an assumption. We need to check the message type for system etc.


        internal::SetUmpMessageType(midiWord, (uint8_t)MidiMessageType::Midi1ChannelVoice32);

        //if (originalMessage.Type() == Windows::Devices::Midi::MidiMessageType::NoteOff) ...

        // set the group
        internal::SetGroupIndexInFirstWord(midiWord, group.Index());

        auto message = winrt::make<MidiMessage32>();
        message.Timestamp(timestamp);
        message.Word0(midiWord);

        return message;

    }

    _Use_decl_annotations_
    midi2::MidiMessage32 MidiMessageConverter::ConvertMidi1Message(
        internal::MidiTimestamp const timestamp,
        midi2::MidiGroup const& group,
        uint8_t const statusByte,
        uint8_t const dataByte1
    ) noexcept
    {
        uint32_t midiWord{ 0 };

        midiWord = internal::MidiWordFromBytes(
            (uint8_t)0x00,
            statusByte,
            internal::CleanupByte7(dataByte1),
            0x00
        );

        // TODO: this is an assumption. We need to check the message type for system etc.


        internal::SetUmpMessageType(midiWord, (uint8_t)MidiMessageType::Midi1ChannelVoice32);
        //if (originalMessage.Type() == Windows::Devices::Midi::MidiMessageType::NoteOff) ...

        // set the group
        internal::SetGroupIndexInFirstWord(midiWord, group.Index());

        auto message = winrt::make<MidiMessage32>();
        message.Timestamp(timestamp);
        message.Word0(midiWord);

        return message;
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
        uint32_t midiWord{ 0 };

        midiWord = internal::MidiWordFromBytes(
            (uint8_t)0x00,
            statusByte,
            internal::CleanupByte7(dataByte1),
            internal::CleanupByte7(dataByte2)
        );

        // TODO: this is an assumption. We need to check the message type for system etc.


        //if (originalMessage.Type() == Windows::Devices::Midi::MidiMessageType::NoteOff)

        internal::SetUmpMessageType(midiWord, (uint8_t)MidiMessageType::Midi1ChannelVoice32);

        // set the group
        internal::SetGroupIndexInFirstWord(midiWord, group.Index());

        auto message = winrt::make<MidiMessage32>();
        message.Timestamp(timestamp);
        message.Word0(midiWord);

        return message;
    }



    _Use_decl_annotations_
    uint32_t MidiMessageConverter::InternalConvertBytes(
        uint8_t const groupIndex,
        Windows::Devices::Midi::IMidiMessage const& originalMessage
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
        internal::SetUmpMessageType(midiWord, (uint8_t)MidiMessageType::Midi1ChannelVoice32);
        //if (originalMessage.Type() == Windows::Devices::Midi::MidiMessageType::NoteOff) ...

        // set the group
        internal::SetGroupIndexInFirstWord(midiWord, groupIndex);

        return midiWord;
    }

}
