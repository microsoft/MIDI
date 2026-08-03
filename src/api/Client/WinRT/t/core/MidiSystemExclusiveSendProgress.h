// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.SysExTransfer.MidiSystemExclusiveSendProgress.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    struct MidiSystemExclusiveSendProgress : MidiSystemExclusiveSendProgressT<MidiSystemExclusiveSendProgress>
    {
        MidiSystemExclusiveSendProgress() = default;

        uint64_t CountBytesRead() const noexcept { return m_countBytesRead; }
        uint64_t CountMessagesSent() const noexcept { return m_countMessagesSent; }

        void InternalInitialize(
            _In_ uint64_t const countBytesRead, 
            _In_ uint64_t const countMessagesSent);

        void InternalIncrementCountBytesRead() { m_countBytesRead++; }
        void InternalIncrementCountMessagesSent() { m_countMessagesSent++; }

    private:
        uint64_t m_countBytesRead{ 0 };
        uint64_t m_countMessagesSent{ 0 };
    };
}
