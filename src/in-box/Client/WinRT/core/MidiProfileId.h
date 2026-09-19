// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiProfileId.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiProfileId : MidiProfileIdT<MidiProfileId>
    {
        MidiProfileId() = default;

        MidiProfileId(
            _In_ uint8_t const idByte1,
            _In_ uint8_t const idByte2,
            _In_ uint8_t const idByte3,
            _In_ uint8_t const idByte4,
            _In_ uint8_t const idByte5) noexcept
        {
            IdByte1(idByte1);
            IdByte2(idByte2);
            IdByte3(idByte3);
            IdByte4(idByte4);
            IdByte5(idByte5);
        }

        static uint8_t StandardDefinedIdByte1() noexcept { return 0x7E; }

        static ci::MidiProfileId CreateStandardDefined(
            _In_ uint8_t const profileBank,
            _In_ uint8_t const profileNumber,
            _In_ uint8_t const profileVersion,
            _In_ uint8_t const profileLevel) noexcept;

        static ci::MidiProfileId CreateManufacturerSpecific(
            _In_ uint8_t const manufacturerSysExIdByte1,
            _In_ uint8_t const manufacturerSysExIdByte2,
            _In_ uint8_t const manufacturerSysExIdByte3,
            _In_ uint8_t const manufacturerInfoByte1,
            _In_ uint8_t const manufacturerInfoByte2) noexcept;

        uint8_t IdByte1() const noexcept { return m_byte1; }
        void IdByte1(_In_ uint8_t const value) noexcept { m_byte1 = internal::CleanupByte7(value); }

        uint8_t IdByte2() const noexcept { return m_byte2; }
        void IdByte2(_In_ uint8_t const value) noexcept { m_byte2 = internal::CleanupByte7(value); }

        uint8_t IdByte3() const noexcept { return m_byte3; }
        void IdByte3(_In_ uint8_t const value) noexcept { m_byte3 = internal::CleanupByte7(value); }

        uint8_t IdByte4() const noexcept { return m_byte4; }
        void IdByte4(_In_ uint8_t const value) noexcept { m_byte4 = internal::CleanupByte7(value); }

        uint8_t IdByte5() const noexcept { return m_byte5; }
        void IdByte5(_In_ uint8_t const value) noexcept { m_byte5 = internal::CleanupByte7(value); }

        bool IsStandardDefined() const noexcept { return m_byte1 == 0x7E; }

        uint8_t ProfileBank() const noexcept { return IsStandardDefined() ? m_byte2 : (uint8_t)0; }
        uint8_t ProfileNumber() const noexcept { return IsStandardDefined() ? m_byte3 : (uint8_t)0; }
        uint8_t ProfileVersion() const noexcept { return IsStandardDefined() ? m_byte4 : (uint8_t)0; }
        uint8_t ProfileLevel() const noexcept { return IsStandardDefined() ? m_byte5 : (uint8_t)0; }

        bool IsSameProfileAs(_In_ ci::MidiProfileId const& other) const noexcept;

        winrt::hstring ToString();

    private:
        uint8_t m_byte1{};
        uint8_t m_byte2{};
        uint8_t m_byte3{};
        uint8_t m_byte4{};
        uint8_t m_byte5{};
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiProfileId : MidiProfileIdT<MidiProfileId, implementation::MidiProfileId>
    {
    };
}
