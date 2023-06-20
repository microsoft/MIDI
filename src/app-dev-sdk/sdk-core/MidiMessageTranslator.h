// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageTranslator.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiMessageTranslator : MidiMessageTranslatorT<MidiMessageTranslator>
    {
        MidiMessageTranslator() = default;

        static com_array<uint32_t> TranslateMidi1ByteCVToMidi2UmpCV(array_view<uint8_t const> midi1Bytes);
        static com_array<uint8_t> TranslateMidi2UmpCVToMidi1ByteCV(array_view<uint32_t const> midi2Words);
        static com_array<uint32_t> TranslateMidi1UmpCVToMidi2UmpCV(uint32_t midi1CVUmp);
        static uint32_t TranslateMidi2UmpCVToMidi1UmpCV(uint32_t midi2CVUmp);
        static uint16_t Scale7BitValueto16BitValue(uint8_t sevenBitValue);
        static uint32_t Scale14BitValueTo32BitValue(uint16_t fourteenBitValue);
        static uint32_t Scale14BitValueTo32BitValue(uint8_t fourteenBitValueMSB, uint8_t fourteenBitValueLSB);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiMessageTranslator : MidiMessageTranslatorT<MidiMessageTranslator, implementation::MidiMessageTranslator>
    {
    };
}
