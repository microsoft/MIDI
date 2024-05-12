// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiUniqueId.h"
#include "MidiUniqueId.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    midi2::MidiUniqueId MidiUniqueId::CreateBroadcast()
    {
        //return winrt::make<MidiUniqueId>(MIDI_MUID_BROADCAST);
        return midi2::MidiUniqueId(MIDI_MUID_BROADCAST);

    }

    midi2::MidiUniqueId MidiUniqueId::CreateRandom()
    {
        std::srand((int)(implementation::MidiClock::Now() & 0x00000000FFFFFFFF));

        uint32_t val = ((uint32_t)(std::rand()) % 0xFFFFF);

        // get us out of the reserved area
        val <<= 8;

        //return winrt::make<MidiUniqueId>(val);
        return midi2::MidiUniqueId(val);
    }

    uint32_t MidiUniqueId::AsCombined28BitValue() const noexcept
    {
        // the MUID is 4 bytes but in LSB->MSB order with Byte1 being the LSB

        uint32_t val;

        val = 
            (uint32_t)Byte1() |
            (uint32_t)Byte2() << 7 |
            (uint32_t)Byte3() << 14 |
            (uint32_t)Byte4() << 21;

        return val;
    }

    _Use_decl_annotations_
    MidiUniqueId::MidiUniqueId(uint32_t const combined28BitValue) noexcept
    {
        // the MUID is 4 bytes but in LSB->MSB order with Byte1 being the LSB

        uint32_t val = combined28BitValue & 0x0FFFFFFF;

        Byte1(val & 0x7F);

        val >>= 7;
        Byte2(val & 0x7F);

        val >>= 7;
        Byte3(val & 0x7F);

        val >>= 7;
        Byte4(val & 0x7F);

    }

}
