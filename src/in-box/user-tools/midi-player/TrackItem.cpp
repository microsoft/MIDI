// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TrackItem.h"
#include "TrackItem.g.cpp"

#include "NoteRollRenderer.h"
#include "StringResources.h"

namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    _Use_decl_annotations_
    void TrackItem::Update(::midiplayer::TrackRowData const& data) noexcept
    {
        try
        {
            m_trackIndex = static_cast<int32_t>(data.TrackIndex);

            m_displayName = data.DisplayName.empty()
                ? res::FormatString(L"TrackUnnamedFormat", data.TrackIndex + 1)
                : winrt::hstring{ data.DisplayName };

            m_patchText = winrt::hstring{ data.PatchName };

            // Channels are shown the way players count them, from 1, and a track playing several
            // is summarized rather than listing all sixteen.
            std::wstring channels{};
            int32_t channelCount = 0;
            int32_t firstChannel = 0;

            for (uint8_t channel = 0; channel < 16; ++channel)
            {
                if ((data.ChannelMask & (1u << channel)) != 0)
                {
                    if (channelCount == 0) { firstChannel = channel + 1; }
                    ++channelCount;
                }
            }

            if (channelCount == 1)
            {
                channels = res::FormatString(L"TrackChannelFormat", firstChannel);
            }
            else if (channelCount > 1)
            {
                channels = res::FormatString(L"TrackChannelCountFormat", channelCount);
            }

            auto const notes = std::wstring{ res::FormatString(L"TrackNoteCountFormat", data.NoteCount) };

            m_detailText = channels.empty()
                ? winrt::hstring{ notes }
                : winrt::hstring{ channels + L"  \u00B7  " + notes };

            m_colorBrush = xaml::Media::SolidColorBrush{
                ::midiplayer::NoteRollRenderer::TrackColor(data.TrackIndex) };

            RaisePropertyChanged(L"TrackIndex");
            RaisePropertyChanged(L"DisplayName");
            RaisePropertyChanged(L"DetailText");
            RaisePropertyChanged(L"PatchText");
            RaisePropertyChanged(L"ColorBrush");
            RaisePropertyChanged(L"RowAccessibleName");
            RaisePropertyChanged(L"MuteAccessibleName");
            RaisePropertyChanged(L"SoloAccessibleName");
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update a track row.")
    }

    void TrackItem::IsMuted(bool value) noexcept
    {
        if (m_isMuted == value)
        {
            return;
        }

        m_isMuted = value;

        RaisePropertyChanged(L"IsMuted");
        RaisePropertyChanged(L"ContentOpacity");
        RaisePropertyChanged(L"MuteAccessibleName");
    }

    void TrackItem::IsSolo(bool value) noexcept
    {
        if (m_isSolo == value)
        {
            return;
        }

        m_isSolo = value;

        RaisePropertyChanged(L"IsSolo");
        RaisePropertyChanged(L"ContentOpacity");
        RaisePropertyChanged(L"SoloAccessibleName");
    }

    void TrackItem::SetSilencedByOther(bool value) noexcept
    {
        if (m_silencedByOther == value)
        {
            return;
        }

        m_silencedByOther = value;

        RaisePropertyChanged(L"ContentOpacity");
    }

    void TrackItem::SetSoundingNoteCount(uint8_t value) noexcept
    {
        // Four notes reads as full. Past that a dense track would sit pinned and tell you nothing.
        auto const level = value == 0 ? 0.0 : (value >= 4 ? 1.0 : 0.25 * value);

        // This runs every frame for every track, so a notification only goes out when the bar
        // would actually be drawn differently.
        if (std::abs(level - m_activityLevel) < 0.01)
        {
            return;
        }

        m_activityLevel = level;

        RaisePropertyChanged(L"ActivityLevel");
    }

    double TrackItem::ContentOpacity() const noexcept
    {
        return (m_isMuted || m_silencedByOther) ? 0.4 : 1.0;
    }

    winrt::hstring TrackItem::RowAccessibleName() const noexcept
    {
        // The row shows three separate text elements; a screen reader announces the container's
        // name only, so they have to be composed into it.
        return res::FormatString(
            L"TrackRowAccessibleNameFormat",
            std::wstring{ m_displayName },
            std::wstring{ m_patchText },
            std::wstring{ m_detailText });
    }

    winrt::hstring TrackItem::MuteAccessibleName() const noexcept
    {
        return res::FormatString(
            m_isMuted ? L"TrackUnmuteAccessibleNameFormat" : L"TrackMuteAccessibleNameFormat",
            std::wstring{ m_displayName });
    }

    winrt::hstring TrackItem::SoloAccessibleName() const noexcept
    {
        return res::FormatString(
            m_isSolo ? L"TrackUnsoloAccessibleNameFormat" : L"TrackSoloAccessibleNameFormat",
            std::wstring{ m_displayName });
    }

    _Use_decl_annotations_
    void TrackItem::RaisePropertyChanged(winrt::hstring const& name) noexcept
    {
        try
        {
            m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ name });
        }
        catch (...)
        {
        }
    }
}
