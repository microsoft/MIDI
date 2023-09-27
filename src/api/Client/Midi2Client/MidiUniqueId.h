// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiUniqueId.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiUniqueId : MidiUniqueIdT<MidiUniqueId>
    {
        MidiUniqueId() = default;

        MidiUniqueId(
            _In_ uint8_t const sevenBitByte1, 
            _In_ uint8_t const sevenBitByte2,
            _In_ uint8_t const sevenBitByte3,
            _In_ uint8_t const sevenBitByte4
        ) noexcept
        {
            Byte1(sevenBitByte1);
            Byte2(sevenBitByte2);
            Byte3(sevenBitByte3);
            Byte4(sevenBitByte4);
        }
        
        uint8_t Byte1() const noexcept { return m_byte1; }
        void Byte1(_In_ uint8_t value) noexcept { m_byte1 = value & 0x7F; }

        uint8_t Byte2() const noexcept { return m_byte2; }
        void Byte2(_In_ uint8_t value) noexcept { m_byte2 = value & 0x7F; }

        uint8_t Byte3() const noexcept { return m_byte3; }
        void Byte3(_In_ uint8_t value) noexcept { m_byte3 = value & 0x7F; }

        uint8_t Byte4() const noexcept { return m_byte4; }
        void Byte4(_In_ uint8_t value) noexcept { m_byte4 = value & 0x7F; }

    private:
        uint8_t m_byte1{};    // byte 1 is LSB
        uint8_t m_byte2{};
        uint8_t m_byte3{};
        uint8_t m_byte4{};
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiUniqueId : MidiUniqueIdT<MidiUniqueId, implementation::MidiUniqueId>
    {
    };
}
