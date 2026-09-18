// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiChannelList.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiChannelList : MidiChannelListT<MidiChannelList>
    {
        MidiChannelList() = default;

        foundation::Collections::IVector<ci::MidiChannelListEntry> Entries() const noexcept { return m_entries; }

        ci::MidiChannelListEntry GetEntryForChannel(_In_ uint16_t const oneBasedChannel) noexcept;
        foundation::Collections::IVector<ci::MidiResourceLink> GetProgramListLinks() noexcept;

        json::JsonArray GetJson() noexcept;
        static ci::MidiChannelList FromJson(_In_ json::JsonArray const& jsonArray) noexcept;

    private:
        foundation::Collections::IVector<ci::MidiChannelListEntry> m_entries
            { winrt::single_threaded_vector<ci::MidiChannelListEntry>() };
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiChannelList : MidiChannelListT<MidiChannelList, implementation::MidiChannelList>
    {
    };
}
