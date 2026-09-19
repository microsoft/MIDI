// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Synth.MidiSynthInstrumentInfo.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    struct MidiSynthInstrumentInfo : MidiSynthInstrumentInfoT<MidiSynthInstrumentInfo>
    {
        MidiSynthInstrumentInfo() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        uint8_t BankMsb() const noexcept { return m_bankMsb; }
        uint8_t BankLsb() const noexcept { return m_bankLsb; }
        uint8_t Program() const noexcept { return m_program; }

        void InternalSet(
            _In_ winrt::hstring const& name,
            _In_ uint8_t const bankMsb,
            _In_ uint8_t const bankLsb,
            _In_ uint8_t const program) noexcept
        {
            m_name = name;
            m_bankMsb = bankMsb;
            m_bankLsb = bankLsb;
            m_program = program;
        }

    private:
        winrt::hstring m_name{};
        uint8_t m_bankMsb{ 0 };
        uint8_t m_bankLsb{ 0 };
        uint8_t m_program{ 0 };
    };
}
