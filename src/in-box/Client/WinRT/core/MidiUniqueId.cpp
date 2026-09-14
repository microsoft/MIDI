// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiUniqueId.h"
#include "CapabilityInquiry.MidiUniqueId.g.cpp"

#include <random>

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    winrt::hstring MidiUniqueId::ToString()
    {
        return winrt::to_hstring(std::format("{} {}", winrt::to_string(MidiUniqueId::LongLabel()), AsCombined28BitValue()));
    }

    ci::MidiUniqueId MidiUniqueId::CreateBroadcast()
    {
        try
        {
            //return winrt::make<MidiUniqueId>(MIDI_MUID_BROADCAST);
            return ci::MidiUniqueId(MIDI_MUID_BROADCAST);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error creating broadcast unique id.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception creating broadcast unique id.");
            return nullptr;
        }
    }

    ci::MidiUniqueId MidiUniqueId::CreateRandom()
    {
        try
        {
            // Draw from the whole range below MIDI_MUID_RESERVED_START, so the reserved values and
            // the broadcast value cannot come out. std::rand() cannot do this: it stops at 32767
            // here, which is a small fraction of the 28 bits the collision odds in the
            // specification assume.
            std::random_device generator;

            std::uniform_int_distribution<uint32_t> distribution(
                MIDI_MUID_MIN_VALUE, MIDI_MUID_RESERVED_START - 1);

            return ci::MidiUniqueId(distribution(generator));
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error creating random unique id.");
            return nullptr;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception creating random unique id.");
            return nullptr;
        }
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
