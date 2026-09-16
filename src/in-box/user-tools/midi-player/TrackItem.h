// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "TrackItem.g.h"
#include "MidiSequence.h"

namespace midiplayer
{
    // Everything a track row shows, gathered so a refresh is one call rather than a dozen setters
    // each raising a change notification.
    struct TrackRowData
    {
        uint16_t TrackIndex{ 0 };
        std::wstring DisplayName{};
        std::wstring PatchName{};
        uint32_t NoteCount{ 0 };
        uint16_t ChannelMask{ 0 };
        bool IsPercussion{ false };
    };
}

namespace winrt::midiplayer::implementation
{
    struct TrackItem : TrackItemT<TrackItem>
    {
        TrackItem() = default;

        void Update(_In_ ::midiplayer::TrackRowData const& data) noexcept;

        int32_t TrackIndex() const noexcept { return m_trackIndex; }
        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring DetailText() const noexcept { return m_detailText; }
        winrt::hstring PatchText() const noexcept { return m_patchText; }

        xaml::Media::Brush ColorBrush() const noexcept { return m_colorBrush; }

        bool IsMuted() const noexcept { return m_isMuted; }
        void IsMuted(bool value) noexcept;

        bool IsSolo() const noexcept { return m_isSolo; }
        void IsSolo(bool value) noexcept;

        // Set when some other track is soloed, so this one reads as silenced without pretending
        // the customer muted it.
        void SetSilencedByOther(bool value) noexcept;

        // Number of notes the track is holding right now. Only raises a change notification when
        // the drawn level actually moves, because this is set on every frame.
        void SetSoundingNoteCount(uint8_t value) noexcept;

        double ActivityLevel() const noexcept { return m_activityLevel; }

        // Implementation side only, so the frame path can tell whether the row is sounding without
        // asking the playback engine and taking its lock once per track per frame.
        bool IsAudible() const noexcept { return !m_isMuted && !m_silencedByOther; }

        double ContentOpacity() const noexcept;

        winrt::hstring MuteAccessibleName() const noexcept;
        winrt::hstring SoloAccessibleName() const noexcept;

        winrt::event_token PropertyChanged(xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        void RaisePropertyChanged(_In_ winrt::hstring const& name) noexcept;

        int32_t m_trackIndex{ 0 };
        winrt::hstring m_displayName{};
        winrt::hstring m_detailText{};
        winrt::hstring m_patchText{};

        xaml::Media::Brush m_colorBrush{ nullptr };

        bool m_isMuted{ false };
        bool m_isSolo{ false };
        bool m_silencedByOther{ false };

        double m_activityLevel{ 0.0 };

        winrt::event<xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};
    };
}

namespace winrt::midiplayer::factory_implementation
{
    struct TrackItem : TrackItemT<TrackItem, implementation::TrackItem>
    {
    };
}
