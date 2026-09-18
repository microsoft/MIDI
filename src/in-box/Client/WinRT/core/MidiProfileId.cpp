// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiProfileId.h"
#include "CapabilityInquiry.MidiProfileId.g.cpp"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    _Use_decl_annotations_
    ci::MidiProfileId MidiProfileId::CreateStandardDefined(
        uint8_t const profileBank,
        uint8_t const profileNumber,
        uint8_t const profileVersion,
        uint8_t const profileLevel) noexcept
    {
        try
        {
            return winrt::make<MidiProfileId>(
                StandardDefinedIdByte1(), profileBank, profileNumber, profileVersion, profileLevel);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    ci::MidiProfileId MidiProfileId::CreateManufacturerSpecific(
        uint8_t const manufacturerSysExIdByte1,
        uint8_t const manufacturerSysExIdByte2,
        uint8_t const manufacturerSysExIdByte3,
        uint8_t const manufacturerInfoByte1,
        uint8_t const manufacturerInfoByte2) noexcept
    {
        try
        {
            return winrt::make<MidiProfileId>(
                manufacturerSysExIdByte1,
                manufacturerSysExIdByte2,
                manufacturerSysExIdByte3,
                manufacturerInfoByte1,
                manufacturerInfoByte2);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    _Use_decl_annotations_
    bool MidiProfileId::IsSameProfileAs(ci::MidiProfileId const& other) const noexcept
    {
        if (other == nullptr)
        {
            return false;
        }

        return
            other.IdByte1() == m_byte1 &&
            other.IdByte2() == m_byte2 &&
            other.IdByte3() == m_byte3 &&
            other.IdByte4() == m_byte4 &&
            other.IdByte5() == m_byte5;
    }

    winrt::hstring MidiProfileId::ToString()
    {
        try
        {
            // The five bytes, in the order they travel. A profile is identified by all five, so
            // showing fewer of them would show something that is not the profile.
            return winrt::to_hstring(
                std::format("{:02X} {:02X} {:02X} {:02X} {:02X}",
                    m_byte1, m_byte2, m_byte3, m_byte4, m_byte5));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return L"";
        }
    }
}
