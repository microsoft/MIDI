// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "MidiMessageViewModel.g.h"
#include "MessageStore.h"
#include "AppSettings.h"

namespace winrt::midi2monitor::implementation
{
    struct MidiMessageViewModel : MidiMessageViewModelT<MidiMessageViewModel>
    {
        MidiMessageViewModel() = default;

        MidiMessageViewModel(
            ::midi2monitor::MessageRecord const& record,
            ::midi2monitor::TimestampDisplayFormat timestampFormat,
            bool showMessageNames,
            double rowFontSize);

        uint64_t Sequence() const noexcept { return m_sequence; }

        winrt::hstring IndexText() const noexcept { return m_indexText; }
        winrt::hstring TimestampText() const noexcept { return m_timestampText; }
        winrt::hstring TimestampUnit() const noexcept { return m_timestampUnit; }
        winrt::hstring GroupText() const noexcept { return m_groupText; }
        winrt::hstring ChannelText() const noexcept { return m_channelText; }
        winrt::hstring DeltaText() const noexcept { return m_deltaText; }
        winrt::hstring DeltaUnit() const noexcept { return m_deltaUnit; }

        winrt::hstring Detail0Label() const noexcept { return m_detailLabels[0]; }
        winrt::hstring Detail0Value() const noexcept { return m_detailValues[0]; }
        winrt::hstring Detail1Label() const noexcept { return m_detailLabels[1]; }
        winrt::hstring Detail1Value() const noexcept { return m_detailValues[1]; }
        winrt::hstring Detail2Label() const noexcept { return m_detailLabels[2]; }
        winrt::hstring Detail2Value() const noexcept { return m_detailValues[2]; }
        winrt::hstring Detail3Label() const noexcept { return m_detailLabels[3]; }
        winrt::hstring Detail3Value() const noexcept { return m_detailValues[3]; }

        winrt::hstring Word0Text() const noexcept { return m_wordText[0]; }
        winrt::hstring Word1Text() const noexcept { return m_wordText[1]; }
        winrt::hstring Word2Text() const noexcept { return m_wordText[2]; }
        winrt::hstring Word3Text() const noexcept { return m_wordText[3]; }

        xaml::Visibility Word1Visibility() const noexcept { return VisibleWhen(m_wordCount > 1); }
        xaml::Visibility Word2Visibility() const noexcept { return VisibleWhen(m_wordCount > 2); }
        xaml::Visibility Word3Visibility() const noexcept { return VisibleWhen(m_wordCount > 3); }

        winrt::hstring MessageName() const noexcept { return m_messageName; }

        // ListViewItemPresenter does not propagate font properties to the row content, so the
        // zoomed sizes have to be bound explicitly rather than inherited from the list.
        double RowFontSize() const noexcept { return m_rowFontSize; }
        double ChicletFontSize() const noexcept { return m_chicletFontSize; }
        double CommentButtonSize() const noexcept { return m_commentButtonSize; }
        double CommentGlyphSize() const noexcept { return m_commentGlyphSize; }
        xaml::Visibility MessageNameVisibility() const noexcept { return VisibleWhen(!m_messageName.empty()); }
        media::Brush MessageNameBackground() const noexcept { return m_messageNameBackground; }
        media::Brush MessageNameForeground() const noexcept { return m_messageNameForeground; }

        media::Brush RowBackground() const noexcept { return m_rowBackground; }

        winrt::hstring NoticeText() const noexcept { return m_noticeText; }
        xaml::Visibility NoticeVisibility() const noexcept { return VisibleWhen(m_isNotice); }
        xaml::Visibility MessageVisibility() const noexcept { return VisibleWhen(!m_isNotice); }

        winrt::hstring Comment() const noexcept { return m_comment; }
        void Comment(winrt::hstring const& value);

        // null rather than an empty string, so rows with no comment show no tooltip at all
        foundation::IInspectable CommentToolTip() const noexcept
        {
            return m_comment.empty() ? nullptr : winrt::box_value(m_comment);
        }

        xaml::Visibility CommentGlyphVisibility() const noexcept { return VisibleWhen(!m_comment.empty()); }

        winrt::event_token PropertyChanged(xaml::Data::PropertyChangedEventHandler const& handler)
        {
            return m_propertyChanged.add(handler);
        }

        void PropertyChanged(winrt::event_token const& token) noexcept
        {
            m_propertyChanged.remove(token);
        }

    private:
        static constexpr xaml::Visibility VisibleWhen(bool condition) noexcept
        {
            return condition ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        uint64_t m_sequence{ 0 };
        uint8_t m_wordCount{ 0 };
        bool m_isNotice{ false };
        double m_rowFontSize{ ::midi2monitor::AppSettings::BaseRowFontSize };
        double m_chicletFontSize{ ::midi2monitor::AppSettings::BaseChicletFontSize };
        double m_commentButtonSize{ ::midi2monitor::AppSettings::BaseCommentButtonSize };
        double m_commentGlyphSize{ ::midi2monitor::AppSettings::BaseCommentGlyphSize };

        winrt::hstring m_indexText{};
        winrt::hstring m_timestampText{};
        winrt::hstring m_timestampUnit{};
        winrt::hstring m_groupText{};
        winrt::hstring m_channelText{};
        winrt::hstring m_deltaText{};
        winrt::hstring m_deltaUnit{};
        winrt::hstring m_messageName{};
        winrt::hstring m_noticeText{};
        winrt::hstring m_comment{};

        std::array<winrt::hstring, 4> m_detailLabels{};
        std::array<winrt::hstring, 4> m_detailValues{};
        std::array<winrt::hstring, 4> m_wordText{};

        media::Brush m_messageNameBackground{ nullptr };
        media::Brush m_messageNameForeground{ nullptr };
        media::Brush m_rowBackground{ nullptr };

        winrt::event<xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};
    };
}

namespace winrt::midi2monitor::factory_implementation
{
    struct MidiMessageViewModel : MidiMessageViewModelT<MidiMessageViewModel, implementation::MidiMessageViewModel>
    {
    };
}
