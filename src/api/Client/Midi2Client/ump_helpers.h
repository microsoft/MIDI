// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

//#include <pch.h>

#define MIDIWORDNIBBLE1(x) ((uint8_t)((x & 0xF0000000) >> 28))
#define MIDIWORDNIBBLE2(x) ((uint8_t)((x & 0x0F000000) >> 24))
#define MIDIWORDNIBBLE3(x) ((uint8_t)((x & 0x00F00000) >> 20))
#define MIDIWORDNIBBLE4(x) ((uint8_t)((x & 0x000F0000) >> 16))

#define UMP32_WORD_COUNT 1
#define UMP64_WORD_COUNT 2
#define UMP96_WORD_COUNT 3
#define UMP128_WORD_COUNT 4

namespace Windows::Devices::Midi2::Internal
{
    inline std::uint8_t GetUmpLengthInMidiWordsFromMessageType(_In_ const std::uint8_t messageType)
    {
        switch (messageType & 0x0F)
        {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x6:
        case 0x7:
            return 1;

        case 0x3:
        case 0x4:
        case 0x8:
        case 0x9:
        case 0xA:
            return 2;

        case 0xB:
        case 0xC:
            return 3;

        case 0x5:
        case 0xD:
        case 0xE:
        case 0xF:
            return 4;

        default:
            return 0;   // with the mask, shouldn't happen. Here only due to the compiler warning
        };

    }
    


    inline void SetUmpMessageType(_In_ std::uint32_t& firstWord, _In_ const uint8_t messageType)
    {
        // first four bits of the word is the message type

        uint32_t t = ((uint32_t)(messageType & 0x0F) << 28);
        
        firstWord = (firstWord & 0x0FFFFFFF) | t;
    }

    inline std::uint8_t GetUmpMessageTypeFromFirstWord(_In_ const std::uint32_t firstWord)
    {
        return (uint8_t)(MIDIWORDNIBBLE1(firstWord));
    }

    inline std::uint8_t GetUmpLengthInMidiWordsFromFirstWord(_In_ const std::uint32_t firstWord)
    {
        return GetUmpLengthInMidiWordsFromMessageType(GetUmpMessageTypeFromFirstWord(firstWord));
    }


    inline std::uint8_t GetUmpLengthInBytesFromFirstWord(_In_ const std::uint32_t firstWord)
    {
        return (uint8_t)(GetUmpLengthInMidiWordsFromFirstWord(firstWord) * sizeof(uint32_t));
    }

#define MIDI_MESSAGE_GROUP_WORD_CLEARING_MASK 0xF0FFFFFF
#define MIDI_MESSAGE_GROUP_WORD_DATA_MASK 0x0F000000
#define MIDI_MESSAGE_GROUP_BITSHIFT 24

#define MIDI_MESSAGE_CHANNEL_WORD_CLEARING_MASK 0xFFF0FFFF
#define MIDI_MESSAGE_CHANNEL_WORD_DATA_MASK 0x000F0000
#define MIDI_MESSAGE_CHANNEL_BITSHIFT 16


    inline std::uint8_t GetGroupIndexFromFirstWord(_In_ const std::uint32_t firstWord)
    {
        return (uint8_t)((firstWord & MIDI_MESSAGE_GROUP_WORD_DATA_MASK) >> MIDI_MESSAGE_GROUP_BITSHIFT);
    }


    // assumes the caller has already checked to see if the message type has a group field. Otherwise, this will stomp on their data
    inline std::uint32_t GetFirstWordWithNewGroup(_In_ std::uint32_t const firstWord, _In_ std::uint8_t const groupIndex)
    {
        if (groupIndex > 15)
            return firstWord;

        // mask out the group
        uint32_t newWord = firstWord & MIDI_MESSAGE_GROUP_WORD_CLEARING_MASK;

        // move our group over to the right position
        uint32_t groupValue = ((uint32_t)groupIndex) << MIDI_MESSAGE_GROUP_BITSHIFT;

        // return the masked word with the new group value
        return newWord | groupValue;
    }

    // NOTE: This check needs to change when new message types are added. The WinRT enum will need to change as well.

    inline bool MessageTypeHasGroupField(_In_ std::uint8_t messageType)
    {
        switch (messageType)
        {
        case 0x0: // MidiUmpMessageType::UtilityMessage32  // type 0
            return false;

        case 0x1: // MidiUmpMessageType::SystemCommon32    // type 1
            return true;
        case 0x2: // MidiUmpMessageType::Midi1ChannelVoice32   // type 2
            return true;
        case 0x3: // MidiUmpMessageType::DataMessage64   // type 3
            return true;
        case 0x4: // MidiUmpMessageType::Midi2ChannelVoice64   // type 4
            return true;
        case 0x5: // MidiUmpMessageType::DataMessage128   // type 5
            return true;
        case 0xD: // MidiUmpMessageType::FlexData128       // type D
            return true;

        case 0xF: // MidiUmpMessageType::UmpStream128      // type F
            return false;


            // all other message types are undefined, so return false until those are defined
        default:
            return false;

        }
    }

    inline std::uint8_t GetChannelIndexFromFirstWord(_In_ const std::uint32_t firstWord)
    {
        return (uint8_t)((firstWord & MIDI_MESSAGE_CHANNEL_WORD_DATA_MASK) >> MIDI_MESSAGE_CHANNEL_BITSHIFT);
    }


    // assumes the caller has already checked to see if the message type has a group field. Otherwise, this will stomp on their data
    inline std::uint32_t GetFirstWordWithNewChannel(_In_ std::uint32_t const firstWord, _In_ std::uint8_t const channelIndex)
    {
        if (channelIndex > 15)
            return firstWord;

        // mask out the channel
        uint32_t newWord = firstWord & MIDI_MESSAGE_CHANNEL_WORD_CLEARING_MASK;

        // move our group over to the right position. Channel is the lower nibble of the second byte
        uint32_t channelValue = ((uint32_t)channelIndex) << MIDI_MESSAGE_CHANNEL_BITSHIFT;

        // return the masked word with the new group value
        return newWord | channelValue;
    }

    inline bool MessageTypeHasChannelField(_In_ std::uint8_t messageType)
    {
        switch (messageType)
        {
        case 0x0: // MidiUmpMessageType::UtilityMessage32  // type 0
            return false;

        case 0x1: // MidiUmpMessageType::SystemCommon32    // type 1
            return true;
        case 0x2: // MidiUmpMessageType::Midi1ChannelVoice32   // type 2
            return true;
        case 0x3: // MidiUmpMessageType::DataMessage64   // type 3 - sysex. Sysex doesn't include channel but does have a group
            return false;
        case 0x4: // MidiUmpMessageType::Midi2ChannelVoice64   // type 4
            return true;
        case 0x5: // MidiUmpMessageType::DataMessage128   // type 5 - sysex. Sysex doesn't include channel but does have a group
            return false;
        case 0xD: // MidiUmpMessageType::FlexData128       // type D
            return true;

        case 0xF: // MidiUmpMessageType::UmpStream128      // type F
            return false;


            // all other message types are undefined, so return false until those are defined
        default:
            return false;

        }
    }



    inline uint16_t CleanupInt10(_In_ uint16_t const value)
    {
        return value & (uint16_t)0x3FF;
    }

    // 7 bit byte
    inline uint8_t CleanupByte7(_In_ uint8_t const value)
    {
        return value & (uint8_t)0x7F;
    }

    // 4-bit value
    inline uint8_t CleanupNibble(_In_ uint8_t const value)
    {
        return value & (uint8_t)0xF;
    }

    // 2-bit value
    inline uint8_t CleanupCrumb(_In_ uint8_t const value)
    {
        return value & (uint8_t)0x3;
    }


    // in order from msb to lsb
    inline uint32_t MidiWordFromBytes(
        _In_ uint8_t byte0,
        _In_ uint8_t byte1,
        _In_ uint8_t byte2,
        _In_ uint8_t byte3
        )
    {
        return (uint32_t)(byte0 << 24 | byte1 << 16 | byte2 << 8 | byte3);
    }

}
