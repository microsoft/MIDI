// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Files.MidiFileWriteResult.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    struct MidiFileWriteResult : MidiFileWriteResultT<MidiFileWriteResult>
    {
        MidiFileWriteResult() = default;

        bool Succeeded() const noexcept { return m_status == Files::MidiFileWriteStatus::Success; }
        Files::MidiFileWriteStatus Status() const noexcept { return m_status; }

        uint32_t TrackCount() const noexcept { return m_trackCount; }
        uint64_t ByteCount() const noexcept { return m_byteCount; }
        uint32_t SkippedEventCount() const noexcept { return m_skippedEventCount; }

        void InternalInitialize(
            _In_ Files::MidiFileWriteStatus const status,
            _In_ uint32_t const trackCount,
            _In_ uint64_t const byteCount,
            _In_ uint32_t const skippedEventCount) noexcept;

    private:
        Files::MidiFileWriteStatus m_status{ Files::MidiFileWriteStatus::WriteError };
        uint32_t m_trackCount{ 0 };
        uint64_t m_byteCount{ 0 };
        uint32_t m_skippedEventCount{ 0 };
    };
}
