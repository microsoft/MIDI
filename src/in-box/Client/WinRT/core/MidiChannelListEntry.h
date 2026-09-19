// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiChannelListEntry.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiChannelListEntry : MidiChannelListEntryT<MidiChannelListEntry>
    {
        MidiChannelListEntry() = default;

        MidiChannelListEntry(
            _In_ uint16_t const channel,
            _In_ winrt::hstring const& title) noexcept
            : m_title(title), m_channel(channel) {}

        winrt::hstring Title() const noexcept { return m_title; }
        void Title(_In_ winrt::hstring const& value) noexcept { m_title = value; }

        uint16_t Channel() const noexcept { return m_channel; }
        void Channel(_In_ uint16_t const value) noexcept { m_channel = value; }

        uint8_t BankMsb() const noexcept { return m_bankMsb; }
        void BankMsb(_In_ uint8_t const value) noexcept { m_bankMsb = value & 0x7F; }

        uint8_t BankLsb() const noexcept { return m_bankLsb; }
        void BankLsb(_In_ uint8_t const value) noexcept { m_bankLsb = value & 0x7F; }

        uint8_t ProgramChange() const noexcept { return m_programChange; }
        void ProgramChange(_In_ uint8_t const value) noexcept { m_programChange = value & 0x7F; }

        winrt::hstring ProgramTitle() const noexcept { return m_programTitle; }
        void ProgramTitle(_In_ winrt::hstring const& value) noexcept { m_programTitle = value; }

        foundation::Collections::IVector<ci::MidiResourceLink> Links() const noexcept { return m_links; }

        json::JsonObject GetJson() noexcept;
        static ci::MidiChannelListEntry FromJson(_In_ json::JsonObject const& jsonObject) noexcept;

        winrt::hstring ToString();

    private:
        winrt::hstring m_title{};

        uint16_t m_channel{ 1 };

        uint8_t m_bankMsb{ 0 };
        uint8_t m_bankLsb{ 0 };
        uint8_t m_programChange{ 0 };

        winrt::hstring m_programTitle{};

        foundation::Collections::IVector<ci::MidiResourceLink> m_links
            { winrt::single_threaded_vector<ci::MidiResourceLink>() };
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiChannelListEntry : MidiChannelListEntryT<MidiChannelListEntry, implementation::MidiChannelListEntry>
    {
    };
}
