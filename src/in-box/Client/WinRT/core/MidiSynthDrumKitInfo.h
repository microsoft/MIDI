// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Synth.MidiSynthDrumKitInfo.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Synth::implementation
{
    struct MidiSynthDrumKitInfo : MidiSynthDrumKitInfoT<MidiSynthDrumKitInfo>
    {
        MidiSynthDrumKitInfo() = default;

        winrt::hstring Name() const noexcept { return m_name; }
        uint8_t Program() const noexcept { return m_program; }

        void InternalSet(_In_ winrt::hstring const& name, _In_ uint8_t const program) noexcept
        {
            m_name = name;
            m_program = program;
        }

    private:
        winrt::hstring m_name{};
        uint8_t m_program{ 0 };
    };
}
