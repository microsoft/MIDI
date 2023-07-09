// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

#define MIDIWORDNIBBLE1(x) ((uint8_t)((x & 0xF0000000) >> 28))
#define MIDIWORDNIBBLE2(x) ((uint8_t)((x & 0x0F000000) >> 24))
#define MIDIWORDNIBBLE3(x) ((uint8_t)((x & 0x00F00000) >> 20))
#define MIDIWORDNIBBLE4(x) ((uint8_t)((x & 0x000F0000) >> 16))


namespace Windows::Devices::Midi2::Internal
{

    inline std::uint8_t GetUmpMessageTypeFromFirstWord(const std::uint32_t firstWord)
	{
		return (uint8_t)(MIDIWORDNIBBLE1(firstWord));
	}

    inline std::uint8_t GetUmpLengthInMidiWordsFromMessageType(const std::uint8_t messageType)
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
        };

    }
    
    inline std::uint8_t GetUmpLengthInMidiWordsFromFirstWord(const std::uint32_t firstWord)
	{
        return GetUmpLengthInMidiWordsFromMessageType(GetUmpMessageTypeFromFirstWord(firstWord));


	}

	inline std::uint8_t GetUmpLengthInBytesFromFirstWord(const std::uint32_t firstWord)
	{
		return (uint8_t)(GetUmpLengthInMidiWordsFromFirstWord(firstWord) * sizeof(uint32_t));
	}


}
