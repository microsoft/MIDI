// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiMessageViewModel.h"
#include "MidiMessageViewModel.g.cpp"

#include "MidiMessageDecoder.h"
#include "MonitorPalette.h"
#include "TimestampFormatter.h"

namespace midi2native = ::midi2monitor;

namespace winrt::midi2monitor::implementation
{
    _Use_decl_annotations_
    MidiMessageViewModel::MidiMessageViewModel(
        midi2native::MessageRecord const& record,
        midi2native::TimestampDisplayFormat timestampFormat,
        bool showMessageNames)
    {
        m_sequence = record.Sequence;
        m_comment = record.Comment;
        m_rowBackground = midi2native::MonitorPalette::RowBackground(record.AlternateBatch);

        if (record.Kind == midi2native::RecordKind::Notice)
        {
            m_isNotice = true;
            m_noticeText = record.NoticeText;
            return;
        }

        m_wordCount = record.WordCount;
        m_indexText = winrt::hstring{ std::format(L"{}", record.MessageIndex) };

        // ticks are absolute; every other unit reads better as an offset from the moment the
        // customer started monitoring
        auto const displayTimestamp = (timestampFormat == midi2native::TimestampDisplayFormat::Ticks)
            ? record.Timestamp
            : record.OffsetTicks;

        auto const timestamp = midi2native::TimestampFormatter::FormatTimestamp(displayTimestamp, timestampFormat);
        m_timestampText = timestamp.Value;
        m_timestampUnit = timestamp.Unit;

        auto const delta = midi2native::TimestampFormatter::FormatDelta(record.DeltaTicks);
        m_deltaText = delta.Value;
        m_deltaUnit = delta.Unit;

        for (uint8_t i = 0; i < record.WordCount && i < 4; i++)
        {
            m_wordText[i] = winrt::hstring{ std::format(L"{:08X}", record.Words[i]) };
        }

        if (record.HasGroup)
        {
            m_groupText = winrt::hstring{ std::format(L"{}", record.GroupNumber) };
        }

        if (record.HasChannel)
        {
            m_channelText = winrt::hstring{ std::format(L"{}", record.ChannelNumber) };
        }

        auto const decoded = midi2native::DecodeMessage(record);

        for (uint32_t i = 0; i < decoded.Count && i < m_detailLabels.size(); i++)
        {
            // the separator lives on the label so the XAML runs can sit directly next to each other
            m_detailLabels[i] = winrt::hstring{
                (i > 0 ? std::wstring{ L"   " } : std::wstring{}) + std::wstring{ decoded.Labels[i] } + L" " };

            m_detailValues[i] = decoded.Values[i];
        }

        if (showMessageNames)
        {
            m_messageName = midi2native::GetMessageDisplayName(record.Words[0]);
        }

        auto const colorIndex = midi2native::GetMessageTypeColorIndex(
            midi2native::GetMessageTypeFromFirstWord(record.Words[0]));

        m_messageNameBackground = midi2native::MonitorPalette::MessageTypeBackground(colorIndex);
        m_messageNameForeground = midi2native::MonitorPalette::MessageTypeForeground(colorIndex);
    }

    _Use_decl_annotations_
    void MidiMessageViewModel::Comment(winrt::hstring const& value)
    {
        if (m_comment == value)
        {
            return;
        }

        m_comment = value;

        try
        {
            m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ L"Comment" });
            m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ L"CommentGlyphVisibility" });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to raise the comment change notification.")
    }
}
