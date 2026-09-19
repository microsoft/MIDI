// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiProgramListEntry.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiProgramListEntry : MidiProgramListEntryT<MidiProgramListEntry>
    {
        MidiProgramListEntry() = default;

        MidiProgramListEntry(
            _In_ winrt::hstring const& title,
            _In_ uint8_t const bankMsb,
            _In_ uint8_t const bankLsb,
            _In_ uint8_t const programChange) noexcept;

        winrt::hstring Title() const noexcept { return m_title; }
        void Title(_In_ winrt::hstring const& value) noexcept { m_title = value; }

        uint8_t BankMsb() const noexcept { return m_bankMsb; }
        void BankMsb(_In_ uint8_t const value) noexcept { m_bankMsb = value & 0x7F; }

        uint8_t BankLsb() const noexcept { return m_bankLsb; }
        void BankLsb(_In_ uint8_t const value) noexcept { m_bankLsb = value & 0x7F; }

        uint8_t ProgramChange() const noexcept { return m_programChange; }
        void ProgramChange(_In_ uint8_t const value) noexcept { m_programChange = value & 0x7F; }

        foundation::Collections::IVector<winrt::hstring> Tags() const noexcept { return m_tags; }
        foundation::Collections::IVector<winrt::hstring> Categories() const noexcept { return m_categories; }

        winrt::hstring CollectionTitle() const noexcept { return m_collectionTitle; }
        void CollectionTitle(_In_ winrt::hstring const& value) noexcept { m_collectionTitle = value; }

        json::JsonObject GetJson() noexcept;
        static ci::MidiProgramListEntry FromJson(_In_ json::JsonObject const& jsonObject) noexcept;

        winrt::hstring ToString();

    private:
        winrt::hstring m_title{};

        uint8_t m_bankMsb{ 0 };
        uint8_t m_bankLsb{ 0 };
        uint8_t m_programChange{ 0 };

        foundation::Collections::IVector<winrt::hstring> m_tags
            { winrt::single_threaded_vector<winrt::hstring>() };

        foundation::Collections::IVector<winrt::hstring> m_categories
            { winrt::single_threaded_vector<winrt::hstring>() };

        winrt::hstring m_collectionTitle{};
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiProgramListEntry : MidiProgramListEntryT<MidiProgramListEntry, implementation::MidiProgramListEntry>
    {
    };
}
