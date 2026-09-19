// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSequenceLyricLine.h"
#include "Utilities.Sequencing.MidiSequenceLyricLine.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    _Use_decl_annotations_
    void MidiSequenceLyricLine::InternalInitialize(
        uint32_t const startTick,
        uint32_t const endTick,
        winrt::hstring const& text) noexcept
    {
        m_startTick = startTick;
        m_endTick = endTick;
        m_text = text;
    }
}
