// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Files.MidiFileReadResult.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    struct MidiFileReadResult : MidiFileReadResultT<MidiFileReadResult>
    {
        MidiFileReadResult() = default;

        bool Succeeded() const noexcept { return m_status == Files::MidiFileReadStatus::Success; }
        Files::MidiFileReadStatus Status() const noexcept { return m_status; }

        bool Truncated() const noexcept { return m_truncated; }

        Sequencing::MidiSequence Sequence() const noexcept { return m_sequence; }

        uint32_t DeclaredTrackCount() const noexcept { return m_declaredTrackCount; }
        uint32_t ReadTrackCount() const noexcept { return m_readTrackCount; }

        void InternalInitialize(
            _In_ Files::MidiFileReadStatus const status,
            _In_ bool const truncated,
            _In_ uint32_t const declaredTrackCount,
            _In_ uint32_t const readTrackCount,
            _In_ Sequencing::MidiSequence const& sequence) noexcept;

    private:
        Files::MidiFileReadStatus m_status{ Files::MidiFileReadStatus::ReadError };
        bool m_truncated{ false };
        uint32_t m_declaredTrackCount{ 0 };
        uint32_t m_readTrackCount{ 0 };
        Sequencing::MidiSequence m_sequence{ nullptr };
    };
}
