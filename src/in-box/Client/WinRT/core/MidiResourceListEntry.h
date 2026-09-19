// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiResourceListEntry.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiResourceListEntry : MidiResourceListEntryT<MidiResourceListEntry>
    {
        MidiResourceListEntry() = default;

        MidiResourceListEntry(_In_ winrt::hstring const& resource) noexcept
            : m_resource(resource) {}

        static winrt::hstring CanSetNone() noexcept { return L"none"; }
        static winrt::hstring CanSetFull() noexcept { return L"full"; }
        static winrt::hstring CanSetPartial() noexcept { return L"partial"; }

        winrt::hstring Resource() const noexcept { return m_resource; }
        void Resource(_In_ winrt::hstring const& value) noexcept { m_resource = value; }

        bool CanGet() const noexcept { return m_canGet; }
        void CanGet(_In_ bool const value) noexcept { m_canGet = value; }

        winrt::hstring CanSet() const noexcept { return m_canSet; }
        void CanSet(_In_ winrt::hstring const& value) noexcept { m_canSet = value; }

        bool CanSubscribe() const noexcept { return m_canSubscribe; }
        void CanSubscribe(_In_ bool const value) noexcept { m_canSubscribe = value; }

        bool CanPaginate() const noexcept { return m_canPaginate; }
        void CanPaginate(_In_ bool const value) noexcept { m_canPaginate = value; }

        bool RequireResourceId() const noexcept { return m_requireResourceId; }
        void RequireResourceId(_In_ bool const value) noexcept { m_requireResourceId = value; }

        foundation::Collections::IVector<winrt::hstring> MediaTypes() const noexcept { return m_mediaTypes; }
        foundation::Collections::IVector<winrt::hstring> Encodings() const noexcept { return m_encodings; }

        json::JsonObject Schema() const noexcept { return m_schema; }
        void Schema(_In_ json::JsonObject const& value) noexcept { m_schema = value; }

        json::JsonObject GetJson() noexcept;
        static ci::MidiResourceListEntry FromJson(_In_ json::JsonObject const& jsonObject) noexcept;

        winrt::hstring ToString();

    private:
        winrt::hstring m_resource{};

        bool m_canGet{ true };
        winrt::hstring m_canSet{ L"none" };
        bool m_canSubscribe{ false };
        bool m_canPaginate{ false };
        bool m_requireResourceId{ false };

        foundation::Collections::IVector<winrt::hstring> m_mediaTypes
            { winrt::single_threaded_vector<winrt::hstring>() };

        foundation::Collections::IVector<winrt::hstring> m_encodings
            { winrt::single_threaded_vector<winrt::hstring>() };

        json::JsonObject m_schema{ nullptr };
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiResourceListEntry : MidiResourceListEntryT<MidiResourceListEntry, implementation::MidiResourceListEntry>
    {
    };
}
