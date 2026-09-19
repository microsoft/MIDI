// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiResourceLink.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiResourceLink : MidiResourceLinkT<MidiResourceLink>
    {
        MidiResourceLink() = default;

        MidiResourceLink(
            _In_ winrt::hstring const& resource,
            _In_ winrt::hstring const& resourceId) noexcept
            : m_resource(resource), m_resourceId(resourceId) {}

        winrt::hstring Resource() const noexcept { return m_resource; }
        void Resource(_In_ winrt::hstring const& value) noexcept { m_resource = value; }

        winrt::hstring ResourceId() const noexcept { return m_resourceId; }
        void ResourceId(_In_ winrt::hstring const& value) noexcept { m_resourceId = value; }

        winrt::hstring Title() const noexcept { return m_title; }
        void Title(_In_ winrt::hstring const& value) noexcept { m_title = value; }

        json::JsonObject GetJson() noexcept;
        static ci::MidiResourceLink FromJson(_In_ json::JsonObject const& jsonObject) noexcept;

        winrt::hstring ToString();

    private:
        winrt::hstring m_resource{};
        winrt::hstring m_resourceId{};
        winrt::hstring m_title{};
    };
}

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::factory_implementation
{
    struct MidiResourceLink : MidiResourceLinkT<MidiResourceLink, implementation::MidiResourceLink>
    {
    };
}
