// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// About this song: the flyout beside the title, with the names and credits the file carries, a
// few basics, and how its text was read. Split from MainWindowPlayback.cpp.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "PlayQueue.h"
#include "StringResources.h"

namespace res = ::midiplayer::resources;

namespace winrt::midiplayer::implementation
{
    namespace
    {
        // A caption above a value, the way the rest of the window labels things. The value can be
        // selected and copied, and is labeled by its caption for a screen reader.
        void AddSongInfoField(
            _In_ controls::StackPanel const& fields,
            _In_ xaml::Style const& captionStyle,
            _In_ std::wstring_view captionKey,
            _In_ winrt::hstring const& value)
        {
            if (value.empty())
            {
                return;
            }

            controls::TextBlock caption{};
            caption.Text(res::GetString(captionKey));
            caption.Style(captionStyle);

            controls::TextBlock text{};
            text.Text(value);
            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.IsTextSelectionEnabled(true);
            xaml::Automation::AutomationProperties::SetLabeledBy(text, caption);

            controls::StackPanel field{};
            field.Spacing(0);
            field.Children().Append(caption);
            field.Children().Append(text);

            fields.Children().Append(field);
        }

        winrt::hstring FromUtf8(_In_ std::string const& value)
        {
            return value.empty() ? winrt::hstring{} : winrt::to_hstring(value);
        }

        std::wstring_view EncodingNameKey(_In_ midifile::TextEncoding encoding) noexcept
        {
            switch (encoding)
            {
            case midifile::TextEncoding::Utf8:        return L"SongInfoEncodingUtf8";
            case midifile::TextEncoding::Latin1:      return L"SongInfoEncodingLatin1";
            case midifile::TextEncoding::Windows1252: return L"SongInfoEncodingWindows1252";
            case midifile::TextEncoding::ShiftJis:    return L"SongInfoEncodingShiftJis";
            case midifile::TextEncoding::Gbk:         return L"SongInfoEncodingGbk";
            case midifile::TextEncoding::Big5:        return L"SongInfoEncodingBig5";
            case midifile::TextEncoding::Korean:      return L"SongInfoEncodingKorean";
            case midifile::TextEncoding::Unicode:     return L"SongInfoEncodingUnicode";
            default:                                  return {};
            }
        }

        std::wstring_view EncodingSourceKey(_In_ midifile::TextEncodingSource source) noexcept
        {
            switch (source)
            {
            case midifile::TextEncodingSource::CodeSetTag:    return L"SongInfoEncodingFromTag";
            case midifile::TextEncodingSource::ByteOrderMark: return L"SongInfoEncodingFromByteOrderMark";
            default:                                          return L"SongInfoEncodingDetected";
            }
        }

        // Explains how the file's text was read, so text that looks wrong has a reason next to it.
        winrt::hstring DescribeEncoding(_In_ midifile::TextEncodingInfo const& info)
        {
            auto const nameKey = EncodingNameKey(info.Encoding);

            if (nameKey.empty())
            {
                return res::GetString(L"SongInfoEncodingAsciiOnly");
            }

            std::wstring description{ res::FormatString(
                L"SongInfoEncodingFormat",
                std::wstring{ res::GetString(nameKey) },
                std::wstring{ res::GetString(EncodingSourceKey(info.Source)) }) };

            // Latin-1 is what the player falls back to when nothing else fits, so it is also what
            // text in an encoding the player doesn't know ends up as.
            if (info.Encoding == midifile::TextEncoding::Latin1 && info.Source == midifile::TextEncodingSource::Detected)
            {
                description += L" ";
                description += res::GetString(L"SongInfoEncodingLatin1Hint");
            }

            return winrt::hstring{ description };
        }

        std::wstring_view FormatKey(_In_ midifile::SequenceFormat format) noexcept
        {
            switch (format)
            {
            case midifile::SequenceFormat::SingleTrack:   return L"SongInfoFormatSingleTrack";
            case midifile::SequenceFormat::MultiSequence: return L"SongInfoFormatMultiSequence";
            default:                                      return L"SongInfoFormatMultiTrack";
            }
        }
    }

    void MainWindow::UpdateSongInfoButton() noexcept
    {
        try
        {
            SongInfoButton().Visibility(m_currentSequence != nullptr
                ? xaml::Visibility::Visible
                : xaml::Visibility::Collapsed);
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to update the song information button.")
    }

    void MainWindow::OnSongInfoShortcut(input::KeyboardAccelerator const&, input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        try
        {
            args.Handled(true);

            if (m_currentSequence != nullptr && SongInfoButton().Visibility() == xaml::Visibility::Visible)
            {
                SongInfoFlyout().ShowAt(SongInfoButton());
            }
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to open the song information.")
    }

    // Filled in each time it opens, so it always describes the file playing now.
    void MainWindow::OnSongInfoFlyoutOpening(foundation::IInspectable const&, foundation::IInspectable const&)
    {
        try
        {
            auto const fields = SongInfoFields();
            fields.Children().Clear();

            auto const sequence = m_currentSequence;

            if (sequence == nullptr)
            {
                SongInfoEncodingText().Text(winrt::hstring{});
                return;
            }

            auto const& info = sequence->SongInfo;

            // The title the file names as such, else the best name the reader found, else the
            // name the queue shows.
            auto title = FromUtf8(!info.Title.empty() ? info.Title : sequence->Title);

            if (title.empty())
            {
                if (auto const current = m_queue.Current(); current.has_value())
                {
                    title = winrt::hstring{ current.value().Title };
                }
            }

            auto const captionStyle = SongInfoPanel().Resources().Lookup(winrt::box_value(L"SongInfoCaptionStyle")).as<xaml::Style>();

            auto add = [&](std::wstring_view captionKey, winrt::hstring const& value)
                {
                    AddSongInfoField(fields, captionStyle, captionKey, value);
                };

            add(L"SongInfoTitleCaption", title);
            add(L"SongInfoComposerCaption", FromUtf8(info.Composer));
            add(L"SongInfoLyricistCaption", FromUtf8(info.Lyricist));
            add(L"SongInfoArtistCaption", FromUtf8(info.Artist));
            add(L"SongInfoCopyrightCaption", FromUtf8(sequence->Copyright));

            add(L"SongInfoFileTypeCaption", res::GetString(FormatKey(sequence->Format)));
            add(L"SongInfoTracksCaption", winrt::hstring{ std::to_wstring(sequence->Tracks.size()) });
            add(L"SongInfoNotesCaption", winrt::hstring{ std::to_wstring(sequence->Notes.size()) });
            add(L"SongInfoLengthCaption", winrt::hstring{ ::midiplayer::FormatDuration(sequence->DurationMicroseconds) });

            SongInfoEncodingText().Text(DescribeEncoding(sequence->TextEncoding));
        }
        MIDI_PLAYER_CATCH_AND_LOG(L"Unable to show the song information.")
    }
}
