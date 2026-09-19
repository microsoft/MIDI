// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiResourceList.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiResourceList : MidiResourceListT<MidiResourceList>
    {
        MidiResourceList() = default;

        foundation::Collections::IVector<ci::MidiResourceListEntry> Entries() const noexcept { return m_entries; }

        ci::MidiResourceListEntry GetEntry(_In_ winrt::hstring const& resource) noexcept;
        bool SupportsResource(_In_ winrt::hstring const& resource) noexcept;

        json::JsonArray GetJson() noexcept;
        static ci::MidiResourceList FromJson(_In_ json::JsonArray const& jsonArray) noexcept;

    private:
        foundation::Collections::IVector<ci::MidiResourceListEntry> m_entries
            { winrt::single_threaded_vector<ci::MidiResourceListEntry>() };
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiResourceList : MidiResourceListT<MidiResourceList, implementation::MidiResourceList>
    {
    };
}
