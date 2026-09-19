// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiProgramList.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiProgramList : MidiProgramListT<MidiProgramList>
    {
        MidiProgramList() = default;

        foundation::Collections::IVector<ci::MidiProgramListEntry> Entries() const noexcept { return m_entries; }

        uint32_t Offset() const noexcept { return m_offset; }
        void Offset(_In_ uint32_t const value) noexcept { m_offset = value; }

        uint32_t TotalCount() const noexcept;
        void TotalCount(_In_ uint32_t const value) noexcept
        {
            m_totalCount = value;
            m_totalCountReported = true;
        }

        bool HasMoreEntries() const noexcept;
        uint32_t NextOffset() const noexcept;

        json::JsonArray GetJson() noexcept;
        static ci::MidiProgramList FromJson(_In_ json::JsonArray const& jsonArray) noexcept;

    private:
        foundation::Collections::IVector<ci::MidiProgramListEntry> m_entries
            { winrt::single_threaded_vector<ci::MidiProgramListEntry>() };

        uint32_t m_offset{ 0 };

        // A device which sends its whole list at once reports no total, in which case the entry
        // count stands in for it.
        uint32_t m_totalCount{ 0 };
        bool m_totalCountReported{ false };
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiProgramList : MidiProgramListT<MidiProgramList, implementation::MidiProgramList>
    {
    };
}
