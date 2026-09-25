// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiFileWriteResult.h"
#include "Utilities.Files.MidiFileWriteResult.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    _Use_decl_annotations_
    void MidiFileWriteResult::InternalInitialize(
        Files::MidiFileWriteStatus const status,
        uint32_t const trackCount,
        uint64_t const byteCount,
        uint32_t const skippedEventCount) noexcept
    {
        m_status = status;
        m_trackCount = trackCount;
        m_byteCount = byteCount;
        m_skippedEventCount = skippedEventCount;
    }
}
