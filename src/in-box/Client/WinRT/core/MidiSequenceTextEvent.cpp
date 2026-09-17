// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSequenceTextEvent.h"
#include "Utilities.Sequencing.MidiSequenceTextEvent.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    _Use_decl_annotations_
    void MidiSequenceTextEvent::InternalInitialize(
        uint32_t const tick,
        uint16_t const trackIndex,
        Sequencing::MidiSequenceTextKind const kind,
        winrt::hstring const& text) noexcept
    {
        m_tick = tick;
        m_trackIndex = trackIndex;
        m_kind = kind;
        m_text = text;
    }
}
