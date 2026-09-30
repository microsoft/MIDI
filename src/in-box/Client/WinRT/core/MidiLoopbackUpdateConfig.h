// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Loopback.MidiLoopbackUpdateConfig.g.h"

#include "MidiLoopbackManager.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    struct MidiLoopbackUpdateConfig : MidiLoopbackUpdateConfigT<MidiLoopbackUpdateConfig>
    {
        MidiLoopbackUpdateConfig() = default;
        MidiLoopbackUpdateConfig(_In_ winrt::guid const& associationId) noexcept : m_associationId(associationId) { }

        winrt::guid TransportId() const noexcept { return implementation::MidiLoopbackManager::TransportId(); }
        json::JsonObject ConfigJson() const noexcept;

        winrt::guid AssociationId() const noexcept { return m_associationId; }

        winrt::hstring EndpointAName() const noexcept { return m_endpointAName; }
        void EndpointAName(_In_ winrt::hstring const& value) noexcept
        {
            m_endpointAName = internal::TrimmedHStringCopy(value);
            m_hasEndpointAName = !m_endpointAName.empty();
        }

        winrt::hstring EndpointADescription() const noexcept { return m_endpointADescription; }
        void EndpointADescription(_In_ winrt::hstring const& value) noexcept
        {
            m_endpointADescription = internal::TrimmedHStringCopy(value);
            m_hasEndpointADescription = true;
        }

        winrt::hstring EndpointAImageFileName() const noexcept { return m_endpointAImageFileName; }
        void EndpointAImageFileName(_In_ winrt::hstring const& value) noexcept
        {
            m_endpointAImageFileName = winrt::hstring{ internal::CleanImageFileName(value.c_str()) };
            m_hasEndpointAImageFileName = true;
        }

        winrt::hstring EndpointBName() const noexcept { return m_endpointBName; }
        void EndpointBName(_In_ winrt::hstring const& value) noexcept
        {
            m_endpointBName = internal::TrimmedHStringCopy(value);
            m_hasEndpointBName = !m_endpointBName.empty();
        }

        winrt::hstring EndpointBDescription() const noexcept { return m_endpointBDescription; }
        void EndpointBDescription(_In_ winrt::hstring const& value) noexcept
        {
            m_endpointBDescription = internal::TrimmedHStringCopy(value);
            m_hasEndpointBDescription = true;
        }

        winrt::hstring EndpointBImageFileName() const noexcept { return m_endpointBImageFileName; }
        void EndpointBImageFileName(_In_ winrt::hstring const& value) noexcept
        {
            m_endpointBImageFileName = winrt::hstring{ internal::CleanImageFileName(value.c_str()) };
            m_hasEndpointBImageFileName = true;
        }

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(_In_ bool const value) noexcept
        {
            m_isMuted = value;
            m_hasIsMuted = true;
        }

        loop::MidiLoopbackFeedbackProtection FeedbackProtection() const noexcept { return m_feedbackProtection; }
        void FeedbackProtection(_In_ loop::MidiLoopbackFeedbackProtection const value) noexcept
        {
            m_feedbackProtection = value;
            m_hasFeedbackProtection = true;
        }

        // Which properties were set, because only those are changed
        bool InternalHasEndpointAName() const noexcept { return m_hasEndpointAName; }
        bool InternalHasEndpointADescription() const noexcept { return m_hasEndpointADescription; }
        bool InternalHasEndpointAImageFileName() const noexcept { return m_hasEndpointAImageFileName; }
        bool InternalHasEndpointBName() const noexcept { return m_hasEndpointBName; }
        bool InternalHasEndpointBDescription() const noexcept { return m_hasEndpointBDescription; }
        bool InternalHasEndpointBImageFileName() const noexcept { return m_hasEndpointBImageFileName; }
        bool InternalHasIsMuted() const noexcept { return m_hasIsMuted; }
        bool InternalHasFeedbackProtection() const noexcept { return m_hasFeedbackProtection; }

        bool InternalChangesEndpointDetails() const noexcept
        {
            return m_hasEndpointAName || m_hasEndpointADescription || m_hasEndpointAImageFileName ||
                m_hasEndpointBName || m_hasEndpointBDescription || m_hasEndpointBImageFileName;
        }

    private:
        winrt::guid m_associationId{};

        winrt::hstring m_endpointAName{};
        winrt::hstring m_endpointADescription{};
        winrt::hstring m_endpointAImageFileName{};
        winrt::hstring m_endpointBName{};
        winrt::hstring m_endpointBDescription{};
        winrt::hstring m_endpointBImageFileName{};
        bool m_isMuted{ false };
        loop::MidiLoopbackFeedbackProtection m_feedbackProtection{ loop::MidiLoopbackFeedbackProtection::Mute };

        bool m_hasEndpointAName{ false };
        bool m_hasEndpointADescription{ false };
        bool m_hasEndpointAImageFileName{ false };
        bool m_hasEndpointBName{ false };
        bool m_hasEndpointBDescription{ false };
        bool m_hasEndpointBImageFileName{ false };
        bool m_hasIsMuted{ false };
        bool m_hasFeedbackProtection{ false };
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Loopback::factory_implementation
{
    struct MidiLoopbackUpdateConfig : MidiLoopbackUpdateConfigT<MidiLoopbackUpdateConfig, implementation::MidiLoopbackUpdateConfig>
    {
    };
}
