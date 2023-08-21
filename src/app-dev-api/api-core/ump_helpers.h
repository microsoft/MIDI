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
