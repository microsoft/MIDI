// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiFileReadResult.h"
#include "Utilities.Files.MidiFileReadResult.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    _Use_decl_annotations_
    void MidiFileReadResult::InternalInitialize(
        Files::MidiFileReadStatus const status,
        bool const truncated,
        uint32_t const declaredTrackCount,
        uint32_t const readTrackCount,
        Sequencing::MidiSequence const& sequence) noexcept
    {
        m_status = status;
        m_truncated = truncated;
        m_declaredTrackCount = declaredTrackCount;
        m_readTrackCount = readTrackCount;
        m_sequence = sequence;
    }
}
