// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The lyrics track (design section 23): its row header, its menu, the editor, the words shown over
// the timeline as they're sung, and LRC export.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DocumentFiles.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;
namespace documents = ::winrt::Microsoft::UI::Xaml::Documents;

namespace winrt::midisequencer::implementation
{
    using namespace documentfiles;

    namespace
    {
        // The same as the other row headers.
        constexpr double LyricsHeaderWidth = 268.0;

        constexpr double LyricsEditorWidth = 560.0;
        constexpr double LyricsEditorTextHeight = 210.0;
        constexpr double LyricsBarMaximumWidth = 960.0;

        constexpr wchar_t LyricsGlyph[] = L"\uE8BD";

        // How the editor places the words it's given.
        enum class Placement : int32_t
        {
            KeepTimes = 0,
            OnNotes = 1,
            BeatApart = 2,
        };

        xaml::GridLength Pixels(double value) noexcept
        {
            return xaml::GridLength{ value, xaml::GridUnitType::Pixel };
        }

        controls::TextBlock Caption(winrt::hstring const& text, media::Brush const& brush)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(brush);
            return block;
        }
    }

    // ---------------------------------------------------------------- the row

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeLyricsHeader(seq::ArrangeRow const& row)
    {
        controls::Grid grid{};
        grid.Width(LyricsHeaderWidth);
        grid.Height(row.Height);
        grid.BorderThickness(xaml::Thickness{ 0, 0, 1, 1 });
        grid.BorderBrush(BrushFor(m_palette.Divider));
        grid.Background(HeaderBackground(row));
        grid.ColumnSpacing(6);
        grid.Padding(xaml::Thickness{ 0, 0, 8, 0 });

        // strip, indent, chevron, icon, name, edit button, light: the same columns as the other
        // headers, so the icon and name line up with theirs.
        for (auto const width : { Pixels(4), Pixels(0), Pixels(14), xaml::GridLength{ 0, xaml::GridUnitType::Auto }, xaml::GridLength{ 1, xaml::GridUnitType::Star }, xaml::GridLength{ 0, xaml::GridUnitType::Auto }, Pixels(7) })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            grid.ColumnDefinitions().Append(column);
        }

        auto const place = [&grid](xaml::FrameworkElement const& element, int32_t column)
        {
            controls::Grid::SetColumn(element, column);
            grid.Children().Append(element);
        };

        auto const& lyrics = m_doc.Lyrics;
        seq::Track const* singer = lyrics ? seq::FindTrack(m_doc, lyrics->TrackId) : nullptr;

        shapes::Rectangle strip{};
        strip.Fill(BrushFor(singer != nullptr ? seq::FromRgb(singer->Color) : m_palette.Text3));
        place(strip, 0);

        controls::FontIcon icon{};
        icon.Glyph(LyricsGlyph);
        icon.FontSize(13);
        icon.Width(16);
        icon.Foreground(BrushFor(m_palette.Text2));
        place(icon, 3);

        controls::StackPanel info{};
        info.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::TextBlock name{};
        name.Text(res::GetString(L"LyricsTrackName"));
        name.FontSize(13);
        name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        name.TextWrapping(xaml::TextWrapping::NoWrap);
        info.Children().Append(name);

        auto const count = lyrics ? lyrics->Syllables.size() : 0;
        auto summary = res::FormatString(count == 1 ? L"LyricsSyllableCountOneFormat" : L"LyricsSyllableCountFormat", count);
        summary = summary + L" \u00B7 " + (singer != nullptr ? res::FormatString(L"LyricsSungByFormat", singer->Name) : res::GetString(L"LyricsSungByNone"));

        controls::TextBlock sub{};
        sub.Text(summary);
        sub.FontSize(11);
        sub.Margin(xaml::Thickness{ 0, 1, 0, 0 });
        sub.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        sub.TextWrapping(xaml::TextWrapping::NoWrap);
        sub.Foreground(BrushFor(m_palette.Text3));
        info.Children().Append(sub);

        place(info, 4);

        controls::Button edit{};
        edit.Style(RootGrid().Resources().Lookup(winrt::box_value(L"SubtleSmallButtonStyle")).as<xaml::Style>());

        controls::FontIcon pencil{};
        pencil.Glyph(L"\uE70F");
        pencil.FontSize(12);
        edit.Content(pencil);

        automation::AutomationProperties::SetName(edit, res::GetString(L"MenuEditLyrics"));
        controls::ToolTipService::SetToolTip(edit, winrt::box_value(res::GetString(L"MenuEditLyrics")));
        edit.Click([weak = get_weak()](auto&&, auto&&)
        {
            if (auto strong = weak.get())
            {
                strong->ShowLyricsEditor();
            }
        });
        place(edit, 5);

        grid.DoubleTapped([weak = get_weak()](auto&&, input::DoubleTappedRoutedEventArgs const& args)
        {
            args.Handled(true);

            if (auto strong = weak.get())
            {
                strong->ShowLyricsEditor();
            }
        });

        grid.RightTapped([weak = get_weak()](foundation::IInspectable const& sender, input::RightTappedRoutedEventArgs const& args)
        {
            try
            {
                auto strong = weak.get();
                auto const anchor = sender.try_as<xaml::FrameworkElement>();

                if (!strong || anchor == nullptr)
                {
                    return;
                }

                args.Handled(true);

                controls::MenuFlyout menu{};
                strong->AddLyricsMenuItems(menu);

                primitives::FlyoutShowOptions options{};
                options.Position(args.GetPosition(anchor));
                menu.ShowAt(anchor, options);
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the lyrics menu.")
        });

        return grid;
    }

    _Use_decl_annotations_
    void MainWindow::AddLyricsMenuItems(controls::MenuFlyout const& menu)
    {
        auto weak = get_weak();

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);

            controls::FontIcon icon{};
            icon.Glyph(glyph);
            item.Icon(icon);

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        add(res::GetString(L"MenuEditLyrics"), L"\uE70F", [weak]() { if (auto s = weak.get()) { s->ShowLyricsEditor(); } });

        controls::ToggleMenuFlyoutItem bar{};
        bar.Text(res::GetString(L"MenuShowLyricsBar"));
        bar.IsChecked(m_lyricsBarShown);
        bar.Click([weak](auto&&, auto&&)
        {
            if (auto s = weak.get())
            {
                s->m_lyricsBarShown = !s->m_lyricsBarShown;
                s->UpdateLyricsBar();
            }
        });
        menu.Items().Append(bar);

        add(res::GetString(L"MenuSaveLyricsAsLrc"), L"\uE74E", [weak]() { if (auto s = weak.get()) { s->OnExportLyricsClick(nullptr, nullptr); } });

        menu.Items().Append(controls::MenuFlyoutSeparator{});
        add(res::GetString(L"MenuRemoveLyrics"), L"\uE74D", [weak]()
        {
            if (auto s = weak.get())
            {
                s->ApplyLyrics(std::nullopt, std::wstring{ res::GetString(L"UndoRemoveLyrics") });
            }
        });
    }

    _Use_decl_annotations_
    void MainWindow::ApplyLyrics(std::optional<seq::LyricsTrack> lyrics, std::wstring const& undoName)
    {
        try
        {
            if (!lyrics && !m_doc.Lyrics)
            {
                return;
            }

            if (lyrics)
            {
                seq::NormalizeLyrics(*lyrics, seq::SequenceLimits{});
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeLyricsChange(m_doc.Lyrics, lyrics));
            m_doc.Lyrics = std::move(lyrics);
            Commit(undoName, std::move(changes));
            UpdateLyricsBar();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the lyrics.")
    }

    // ---------------------------------------------------------------- the editor

    void MainWindow::ShowLyricsEditor()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const& doc = strong->m_doc;
                auto const existing = doc.Lyrics;
                auto const text2 = strong->BrushFor(strong->m_palette.Text2);

                controls::StackPanel panel{};
                panel.Spacing(12);
                panel.Width(LyricsEditorWidth);

                panel.Children().Append(Caption(res::GetString(L"LyricsEditorHelp"), text2));

                controls::TextBox words{};
                words.AcceptsReturn(true);
                words.TextWrapping(xaml::TextWrapping::Wrap);
                words.Height(LyricsEditorTextHeight);
                words.FontSize(15);
                words.Header(winrt::box_value(res::GetString(L"LyricsWordsHeader")));
                words.PlaceholderText(res::GetString(L"LyricsWordsPlaceholder"));
                words.Text(existing ? winrt::hstring{ seq::LyricsToText(existing->Syllables) } : winrt::hstring{});
                controls::ScrollViewer::SetVerticalScrollBarVisibility(words, controls::ScrollBarVisibility::Auto);
                automation::AutomationProperties::SetName(words, res::GetString(L"LyricsWordsHeader"));
                panel.Children().Append(words);

                // Who sings them: any track that plays notes.
                controls::Grid row{};
                row.ColumnSpacing(12);

                for (auto const width : { xaml::GridLength{ 1, xaml::GridUnitType::Star }, xaml::GridLength{ 160, xaml::GridUnitType::Pixel } })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                controls::ComboBox singer{};
                singer.Header(winrt::box_value(res::GetString(L"LyricsSingerHeader")));
                singer.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                automation::AutomationProperties::SetName(singer, res::GetString(L"LyricsSingerHeader"));
                singer.Items().Append(winrt::box_value(res::GetString(L"LyricsSingerNone")));

                std::vector<std::wstring> singerIds{ std::wstring{} };
                auto const wanted = existing && seq::FindTrack(doc, existing->TrackId) != nullptr ? existing->TrackId : strong->m_selectedTrackId;
                int32_t selected{ 0 };

                seq::ForEachTrack(doc, [&](seq::Track const& track, size_t)
                {
                    if (!track.IsFolder)
                    {
                        if (track.Id == wanted)
                        {
                            selected = static_cast<int32_t>(singerIds.size());
                        }

                        singerIds.push_back(track.Id);
                        singer.Items().Append(winrt::box_value(winrt::hstring{ track.Name }));
                    }

                    return true;
                });

                singer.SelectedIndex(selected);
                controls::Grid::SetColumn(singer, 0);
                row.Children().Append(singer);

                controls::TextBox language{};
                language.Header(winrt::box_value(res::GetString(L"LyricsLanguageHeader")));
                language.PlaceholderText(res::GetString(L"LyricsLanguagePlaceholder"));
                language.MaxLength(35);
                language.Text(existing ? winrt::hstring{ existing->Language } : winrt::hstring{});
                automation::AutomationProperties::SetName(language, res::GetString(L"LyricsLanguageHeader"));
                controls::Grid::SetColumn(language, 1);
                row.Children().Append(language);

                panel.Children().Append(row);

                controls::RadioButtons placement{};
                placement.Header(winrt::box_value(res::GetString(L"LyricsPlacementHeader")));
                automation::AutomationProperties::SetName(placement, res::GetString(L"LyricsPlacementHeader"));

                auto const keepable = existing && !existing->Syllables.empty();

                if (keepable)
                {
                    placement.Items().Append(winrt::box_value(res::GetString(L"LyricsPlaceKeep")));
                }

                placement.Items().Append(winrt::box_value(res::GetString(L"LyricsPlaceOnNotes")));
                placement.Items().Append(winrt::box_value(res::GetString(L"LyricsPlaceBeats")));
                placement.SelectedIndex(0);
                panel.Children().Append(placement);

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::GetString(L"LyricsEditorTitle")));
                dialog.Content(scroller);
                dialog.PrimaryButtonText(res::GetString(L"DialogOk"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(LyricsEditorWidth + 64.0));

                if (keepable)
                {
                    dialog.SecondaryButtonText(res::GetString(L"MenuRemoveLyrics"));
                }

                auto const result = co_await dialog.ShowAsync();

                if (result == controls::ContentDialogResult::Secondary)
                {
                    strong->ApplyLyrics(std::nullopt, std::wstring{ res::GetString(L"UndoRemoveLyrics") });
                    co_return;
                }

                if (result != controls::ContentDialogResult::Primary)
                {
                    co_return;
                }

                // The sequence may have changed while the dialog was open, so read it again.
                auto const& now = strong->m_doc;
                auto syllables = seq::ParseLyricsText(std::wstring_view{ words.Text() }, seq::SequenceLimits{}.MaximumLyrics);

                if (syllables.empty())
                {
                    strong->ApplyLyrics(std::nullopt, std::wstring{ res::GetString(L"UndoRemoveLyrics") });
                    co_return;
                }

                auto const singerIndex = singer.SelectedIndex();
                auto const trackId = singerIndex > 0 && static_cast<size_t>(singerIndex) < singerIds.size() ? singerIds[static_cast<size_t>(singerIndex)] : std::wstring{};

                auto choice = static_cast<Placement>(placement.SelectedIndex() + (keepable ? 0 : 1));

                if (choice == Placement::KeepTimes && now.Lyrics && !now.Lyrics->Syllables.empty())
                {
                    // Those that were there keep their times; any more go on the notes after them.
                    auto const& before = now.Lyrics->Syllables;
                    auto const kept = std::min(before.size(), syllables.size());

                    for (size_t i = 0; i < kept; ++i)
                    {
                        syllables[i].Tick = before[i].Tick;
                    }

                    if (kept < syllables.size())
                    {
                        std::vector<seq::Lyric> added(syllables.begin() + static_cast<ptrdiff_t>(kept), syllables.end());
                        seq::PlaceLyrics(added, now, trackId, before[kept - 1].Tick + 1);
                        std::copy(added.begin(), added.end(), syllables.begin() + static_cast<ptrdiff_t>(kept));
                    }
                }
                else if (choice == Placement::BeatApart)
                {
                    auto const start = seq::SnapTick(strong->m_position, seq::TicksPerQuarterNote);

                    for (size_t i = 0; i < syllables.size(); ++i)
                    {
                        syllables[i].Tick = start + static_cast<int64_t>(i) * seq::TicksPerQuarterNote;
                    }
                }
                else
                {
                    seq::PlaceLyrics(syllables, now, trackId, 0);
                }

                seq::LyricsTrack lyrics{};
                lyrics.Language = std::wstring{ language.Text() };
                lyrics.TrackId = trackId;
                lyrics.Syllables = std::move(syllables);

                if (now.Lyrics)
                {
                    lyrics.RubyLanguage = now.Lyrics->RubyLanguage;
                    lyrics.Unknown = now.Lyrics->Unknown;
                }

                strong->ApplyLyrics(std::move(lyrics), std::wstring{ res::GetString(L"UndoEditLyrics") });
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to edit the lyrics.")
        }(get_strong());
    }

    _Use_decl_annotations_
    void MainWindow::OnLyricsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowLyricsEditor();
    }

    // ---------------------------------------------------------------- the words as they're sung

    void MainWindow::UpdateLyricsBar() noexcept
    {
        try
        {
            auto const hide = [this]()
            {
                if (m_lyricsBar != nullptr)
                {
                    m_lyricsBar.Visibility(xaml::Visibility::Collapsed);
                }

                m_lyricsBarLineIndex = SIZE_MAX;
                m_lyricsBarSungCount = SIZE_MAX;
            };

            if (!m_doc.Lyrics || m_doc.Lyrics->Syllables.empty())
            {
                m_lyricLines.clear();
                m_lyricLinesData = nullptr;
                hide();
                return;
            }

            auto const& syllables = m_doc.Lyrics->Syllables;

            if (m_lyricLinesVersion != m_version || m_lyricLinesData != syllables.data())
            {
                m_lyricLines = seq::LyricLines(syllables);
                m_lyricLinesVersion = m_version;
                m_lyricLinesData = syllables.data();
                m_lyricsBarLineIndex = SIZE_MAX;
                m_lyricsBarSungCount = SIZE_MAX;
            }

            if (!m_lyricsBarShown || m_lyricLines.empty() || m_position >= m_lyricLines.back().EndTick)
            {
                hide();
                return;
            }

            auto const index = seq::LyricLineAtTick(m_lyricLines, m_position);
            auto const& line = m_lyricLines[index];

            size_t sung{ 0 };

            while (sung < line.Count && syllables[line.First + sung].Tick <= m_position)
            {
                ++sung;
            }

            if (index == m_lyricsBarLineIndex && sung == m_lyricsBarSungCount && m_lyricsBar != nullptr)
            {
                return;
            }

            if (m_lyricsBar == nullptr)
            {
                m_lyricsBarLine = controls::TextBlock{};
                m_lyricsBarLine.FontSize(22);
                m_lyricsBarLine.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                m_lyricsBarLine.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                m_lyricsBarLine.TextWrapping(xaml::TextWrapping::NoWrap);
                m_lyricsBarLine.HorizontalAlignment(xaml::HorizontalAlignment::Center);

                m_lyricsBarNext = controls::TextBlock{};
                m_lyricsBarNext.FontSize(14);
                m_lyricsBarNext.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                m_lyricsBarNext.TextWrapping(xaml::TextWrapping::NoWrap);
                m_lyricsBarNext.HorizontalAlignment(xaml::HorizontalAlignment::Center);

                m_lyricsBarCells = controls::StackPanel{};
                m_lyricsBarCells.Orientation(controls::Orientation::Horizontal);
                m_lyricsBarCells.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                m_lyricsBarCells.Visibility(xaml::Visibility::Collapsed);

                controls::StackPanel lines{};
                lines.Spacing(2);
                lines.Margin(xaml::Thickness{ 20, 8, 20, 10 });
                lines.Children().Append(m_lyricsBarLine);
                lines.Children().Append(m_lyricsBarCells);
                lines.Children().Append(m_lyricsBarNext);

                // A Rectangle, not a Border: a Border's rounded corners come out stepped at 125%
                // and 150% scaling.
                m_lyricsBarPlate = shapes::Rectangle{};
                m_lyricsBarPlate.RadiusX(8);
                m_lyricsBarPlate.RadiusY(8);
                m_lyricsBarPlate.StrokeThickness(1);
                m_lyricsBarPlate.UseLayoutRounding(false);

                m_lyricsBar = controls::Grid{};
                m_lyricsBar.Children().Append(m_lyricsBarPlate);
                m_lyricsBar.Children().Append(lines);
                m_lyricsBar.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                m_lyricsBar.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                m_lyricsBar.Margin(xaml::Thickness{ 12, 0, 24, 28 });
                m_lyricsBar.MaxWidth(LyricsBarMaximumWidth);
                m_lyricsBar.IsHitTestVisible(false);
                automation::AutomationProperties::SetName(m_lyricsBar, res::GetString(L"LyricsTrackName"));

                ScrollLaneHost().Children().Append(m_lyricsBar);
            }

            // Colors each time, so a change of theme shows at the next line.
            m_lyricsBarPlate.Fill(BrushFor(m_palette.Light ? seq::Rgba(255, 255, 255, 0.94) : seq::Rgba(32, 32, 32, 0.94)));
            m_lyricsBarPlate.Stroke(BrushFor(m_palette.StrokeStrong));
            m_lyricsBarNext.Foreground(BrushFor(m_palette.Text3));

            // A line with ruby shows each syllable with its reading above it. Ruby can't sit
            // over a run of text, so those lines are built a syllable at a time.
            auto const first = syllables.begin() + static_cast<ptrdiff_t>(line.First);
            auto const hasRuby = std::any_of(first, first + static_cast<ptrdiff_t>(line.Count), [](seq::Lyric const& syllable) { return !syllable.Ruby.empty(); });

            m_lyricsBarLine.Visibility(hasRuby ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
            m_lyricsBarCells.Visibility(hasRuby ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            m_lyricsBarCells.Children().Clear();
            m_lyricsBarLine.Inlines().Clear();

            if (hasRuby)
            {
                for (size_t i = 0; i < line.Count; ++i)
                {
                    auto const& syllable = syllables[line.First + i];
                    auto const brush = BrushFor(i < sung ? m_palette.Accent : m_palette.Text);

                    controls::TextBlock ruby{};
                    ruby.FontSize(11);
                    ruby.Text(winrt::hstring{ syllable.Ruby });
                    ruby.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                    ruby.Foreground(brush);

                    controls::TextBlock text{};
                    text.FontSize(22);
                    text.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                    text.Text(winrt::hstring{ syllable.Text });
                    text.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                    text.Foreground(brush);

                    controls::StackPanel cell{};
                    cell.Children().Append(ruby);
                    cell.Children().Append(text);

                    // A space between words, as the plain line has.
                    if (syllable.End != seq::LyricEnd::None && i + 1 < line.Count)
                    {
                        cell.Margin(xaml::Thickness{ 0, 0, 6, 0 });
                    }

                    m_lyricsBarCells.Children().Append(cell);
                }
            }
            else
            {
                auto const [done, rest] = seq::LyricLineText(syllables, line, m_position);

                documents::Run sungRun{};
                sungRun.Text(winrt::hstring{ done });
                sungRun.Foreground(BrushFor(m_palette.Accent));
                m_lyricsBarLine.Inlines().Append(sungRun);

                documents::Run restRun{};
                restRun.Text(winrt::hstring{ rest });
                restRun.Foreground(BrushFor(m_palette.Text));
                m_lyricsBarLine.Inlines().Append(restRun);
            }

            m_lyricsBarNext.Text(index + 1 < m_lyricLines.size()
                ? winrt::hstring{ seq::LyricLineText(syllables, m_lyricLines[index + 1], INT64_MIN).second }
                : winrt::hstring{});
            m_lyricsBarNext.Visibility(m_lyricsBarNext.Text().empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            m_lyricsBar.Visibility(xaml::Visibility::Visible);

            m_lyricsBarLineIndex = index;
            m_lyricsBarSungCount = sung;

            // The row lights the syllable being sung too.
            if (m_pinnedLaneCanvas != nullptr)
            {
                m_pinnedLaneCanvas.Invalidate();
            }
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- LRC

    _Use_decl_annotations_
    void MainWindow::OnExportLyricsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (!m_doc.Lyrics || m_doc.Lyrics->Syllables.empty())
            {
                ShowMessage(res::GetString(L"LyricsNothingToExport"));
                return;
            }

            auto const name = SafeFileName(m_doc.Name.empty() ? std::wstring{ res::GetString(L"UntitledSequence") } : m_doc.Name);
            auto const path = ShowFileDialog(true, name + L".lrc", {
                { std::wstring{ res::GetString(L"FileTypeLyrics") }, L"*.lrc" } });

            if (path.empty())
            {
                return;
            }

            auto const lrc = seq::LyricsToLrc(*m_doc.Lyrics, m_doc.Tempo, m_doc.Name);

            if (!WriteAllBytes(path, lrc.data(), lrc.size()))
            {
                ShowMessage(res::FormatString(L"ExportFailedFormat", FileStem(path)));
                return;
            }

            ShowMessage(res::FormatString(L"ExportedFormat", FileStem(path)));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to export the lyrics.")
    }
}
