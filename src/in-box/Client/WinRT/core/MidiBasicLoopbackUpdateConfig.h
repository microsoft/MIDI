// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackUpdateConfig.g.h"

#include "MidiBasicLoopbackManager.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackUpdateConfig : MidiBasicLoopbackUpdateConfigT<MidiBasicLoopbackUpdateConfig>
    {
        MidiBasicLoopbackUpdateConfig() = default;
        MidiBasicLoopbackUpdateConfig(_In_ winrt::guid const& associationId) noexcept : m_associationId(associationId) { }

        winrt::guid TransportId() const noexcept { return implementation::MidiBasicLoopbackManager::TransportId(); }
        json::JsonObject ConfigJson() const noexcept;

        winrt::guid AssociationId() const noexcept { return m_associationId; }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept
        {
            m_name = internal::TruncateToUtf8ByteCount(internal::TrimmedWStringCopy(value.c_str()), MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT);
            m_hasName = !m_name.empty();
        }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept
        {
            m_description = internal::TrimmedHStringCopy(value);
            m_hasDescription = true;
        }

        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }
        void ImageFileName(_In_ winrt::hstring const& value) noexcept
        {
            m_imageFileName = winrt::hstring{ internal::CleanImageFileName(value.c_str()) };
            m_hasImageFileName = true;
        }

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(_In_ bool const value) noexcept
        {
            m_isMuted = value;
            m_hasIsMuted = true;
        }

        bloop::MidiBasicLoopbackFeedbackProtection FeedbackProtection() const noexcept { return m_feedbackProtection; }
        void FeedbackProtection(_In_ bloop::MidiBasicLoopbackFeedbackProtection const value) noexcept
        {
            m_feedbackProtection = value;
            m_hasFeedbackProtection = true;
        }

        // Which properties were set, because only those are changed
        bool InternalHasName() const noexcept { return m_hasName; }
        bool InternalHasDescription() const noexcept { return m_hasDescription; }
        bool InternalHasImageFileName() const noexcept { return m_hasImageFileName; }
        bool InternalHasIsMuted() const noexcept { return m_hasIsMuted; }
        bool InternalHasFeedbackProtection() const noexcept { return m_hasFeedbackProtection; }

        bool InternalChangesEndpointDetails() const noexcept { return m_hasName || m_hasDescription || m_hasImageFileName; }

    private:
        winrt::guid m_associationId{};

        winrt::hstring m_name{};
        winrt::hstring m_description{};
        winrt::hstring m_imageFileName{};
        bool m_isMuted{ false };
        bloop::MidiBasicLoopbackFeedbackProtection m_feedbackProtection{ bloop::MidiBasicLoopbackFeedbackProtection::Mute };

        bool m_hasName{ false };
        bool m_hasDescription{ false };
        bool m_hasImageFileName{ false };
        bool m_hasIsMuted{ false };
        bool m_hasFeedbackProtection{ false };
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackUpdateConfig : MidiBasicLoopbackUpdateConfigT<MidiBasicLoopbackUpdateConfig, implementation::MidiBasicLoopbackUpdateConfig>
    {
    };
}
