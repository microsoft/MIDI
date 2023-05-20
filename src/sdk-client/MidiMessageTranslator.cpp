// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiMessageTranslator.h"
#include "MidiMessageTranslator.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    com_array<uint32_t> MidiMessageTranslator::TranslateMidi1ByteCVToMidi2UmpCV(array_view<uint8_t const> midi1Bytes)
    {
        throw hresult_not_implemented();
    }
    com_array<uint8_t> MidiMessageTranslator::TranslateMidi2UmpCVToMidi1ByteCV(array_view<uint32_t const> midi2Words)
    {
        throw hresult_not_implemented();
    }
    com_array<uint32_t> MidiMessageTranslator::TranslateMidi1UmpCVToMidi2UmpCV(uint32_t midi1CVUmp)
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageTranslator::TranslateMidi2UmpCVToMidi1UmpCV(uint32_t midi2CVUmp)
    {
        throw hresult_not_implemented();
    }
    uint16_t MidiMessageTranslator::Scale7BitValueto16BitValue(uint8_t sevenBitValue)
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageTranslator::Scale14BitValueTo32BitValue(uint16_t fourteenBitValue)
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiMessageTranslator::Scale14BitValueTo32BitValue(uint8_t fourteenBitValueMSB, uint8_t fourteenBitValueLSB)
    {
        throw hresult_not_implemented();
    }
}
