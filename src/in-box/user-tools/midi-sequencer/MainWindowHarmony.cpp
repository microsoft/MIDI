// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The Key and chords row (Pete's proposal of November 1 2026, revision 2, phase 1): showing and
// hiding it, its header, picking and dragging chords and keys the way clips are, adding them with
// a double click, and the chord and key pickers. Suggestions, hearing chords and following the key
// come in later phases.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "Harmony.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double HarmonyDragThreshold = 4.0;
        constexpr double HarmonyResizeEdge = 6.0;
        constexpr double TicksPerFourFourBar = static_cast<double>(seq::TicksPerQuarterNote * 4);

        constexpr wchar_t const* KeyScaleNameKeys[]{
            L"KeyScaleMajor", L"KeyScaleMinor", L"KeyScaleDorian", L"KeyScalePhrygian", L"KeyScaleLydian", L"KeyScaleMixolydian",
            L"KeyScaleHarmonicMinor", L"KeyScaleMajorPentatonic", L"KeyScaleMinorPentatonic", L"KeyScaleBlues", L"KeyScaleChromatic",
        };

        constexpr wchar_t const* ScaleNameKeys[]{
            L"ScaleMajor", L"ScaleMinor", L"ScaleDorian", L"ScalePhrygian", L"ScaleLydian", L"ScaleMixolydian",
            L"ScaleHarmonicMinor", L"ScaleMajorPentatonic", L"ScaleMinorPentatonic", L"ScaleBlues", L"ScaleChromatic",
        };

        // The types the picker shows as buttons; the rest are under More.
        constexpr seq::ChordType MainTypes[]{
            seq::ChordType::Major, seq::ChordType::Minor, seq::ChordType::Seventh, seq::ChordType::Major7,
            seq::ChordType::Minor7, seq::ChordType::Diminished, seq::ChordType::Sus4, seq::ChordType::Sus2,
        };

        constexpr seq::ChordType MoreTypes[]{
            seq::ChordType::Sixth, seq::ChordType::Minor6, seq::ChordType::Ninth, seq::ChordType::Major9, seq::ChordType::Minor9,
            seq::ChordType::Add9, seq::ChordType::Augmented, seq::ChordType::Diminished7, seq::ChordType::HalfDiminished,
            seq::ChordType::SeventhSus4, seq::ChordType::Eleventh, seq::ChordType::Thirteenth,
        };

        double PixelsPerTick(double barWidth) noexcept
        {
            return barWidth / TicksPerFourFourBar;
        }
    }

    // ---------------------------------------------------------------- showing the row

    _Use_decl_annotations_
    void MainWindow::OnKeyAndChordsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowHarmonyRow();
    }

    std::vector<winrt::hstring> const& MainWindow::KeyScaleNames()
    {
        if (m_keyScaleNames.empty())
        {
            for (auto const key : KeyScaleNameKeys)
            {
                m_keyScaleNames.push_back(res::GetString(key));
            }
        }

        return m_keyScaleNames;
    }

    xaml::UIElement MainWindow::MakeRomanNumeralsSetting()
    {
        controls::ToggleSwitch toggle{};
        toggle.Header(winrt::box_value(res::GetString(L"RomanNumeralsHeader")));
        toggle.OnContent(winrt::box_value(res::GetString(L"AutoSaveOn")));
        toggle.OffContent(winrt::box_value(res::GetString(L"AutoSaveOff")));
        toggle.IsOn(seq::AppSettings::Current().ShowRomanNumerals());
        toggle.Margin(xaml::Thickness{ 0, 4, 0, 0 });
        controls::ToolTipService::SetToolTip(toggle, winrt::box_value(res::GetString(L"RomanNumeralsHelp")));
        automation::AutomationProperties::SetHelpText(toggle, res::GetString(L"RomanNumeralsHelp"));

        toggle.Toggled([weak = get_weak()](foundation::IInspectable const& sender, auto&&)
        {
            if (auto strong = weak.get())
            {
                seq::AppSettings::Current().ShowRomanNumerals(sender.as<controls::ToggleSwitch>().IsOn());
                strong->InvalidateArrange();
            }
        });

        return toggle;
    }

    void MainWindow::ShowHarmonyRow() noexcept
    {
        try
        {
            if (m_doc.Harmony && !m_doc.Harmony->Hidden)
            {
                return;
            }

            auto after = m_doc.Harmony.value_or(seq::HarmonyTrack{});
            after.Hidden = false;

            // A new row starts in C major, so its chords have numerals from the start.
            if (!m_doc.Harmony)
            {
                after.Keys.push_back(seq::KeyChange{ 0, 0, seq::ScaleKind::Major });
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeHarmonyChange(m_doc.Harmony, after));
            m_doc.Harmony = std::move(after);
            Commit(std::wstring{ res::GetString(L"UndoShowKeyAndChords") }, std::move(changes));
            RebuildLayout();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the Key and chords row.")
    }

    void MainWindow::HideHarmonyRow() noexcept
    {
        try
        {
            if (!m_doc.Harmony || m_doc.Harmony->Hidden)
            {
                return;
            }

            auto after = *m_doc.Harmony;
            after.Hidden = true;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeHarmonyChange(m_doc.Harmony, after));
            m_doc.Harmony = std::move(after);
            m_selectedChord = SIZE_MAX;
            m_selectedKey = SIZE_MAX;
            Commit(std::wstring{ res::GetString(L"UndoHideKeyAndChords") }, std::move(changes));
            RebuildLayout();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to hide the Key and chords row.")
    }

    _Use_decl_annotations_
    void MainWindow::EditHarmony(winrt::hstring const& undoName, std::function<void(seq::HarmonyTrack&)> const& edit) noexcept
    {
        try
        {
            if (!m_doc.Harmony)
            {
                return;
            }

            auto before = m_doc.Harmony;
            auto after = *m_doc.Harmony;
            edit(after);
            seq::NormalizeHarmony(after);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeHarmonyChange(std::move(before), after));
            m_doc.Harmony = std::move(after);
            Commit(std::wstring{ undoName }, std::move(changes));
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the Key and chords row.")
    }

    // ---------------------------------------------------------------- the header

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeHarmonyHeader(seq::ArrangeRow const& row)
    {
        controls::Grid grid{};
        grid.Width(m_headerWidth);
        grid.Height(row.Height);
        grid.BorderThickness(xaml::Thickness{ 0, 0, 1, 1 });
        grid.BorderBrush(BrushFor(m_palette.Divider));
        grid.Background(HeaderBackground(row));
        grid.ColumnSpacing(6);
        grid.Padding(xaml::Thickness{ 0, 0, 8, 0 });

        // strip, indent, chevron, icon, name, buttons, light: the same columns as the other
        // headers, so the icon and name line up with theirs.
        for (auto const width : { xaml::GridLength{ 4, xaml::GridUnitType::Pixel }, xaml::GridLength{ 0, xaml::GridUnitType::Pixel },
                 xaml::GridLength{ 14, xaml::GridUnitType::Pixel }, xaml::GridLength{ 0, xaml::GridUnitType::Auto },
                 xaml::GridLength{ 1, xaml::GridUnitType::Star }, xaml::GridLength{ 0, xaml::GridUnitType::Auto }, xaml::GridLength{ 7, xaml::GridUnitType::Pixel } })
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

        shapes::Rectangle strip{};
        strip.Fill(BrushFor(m_palette.Accent));
        place(strip, 0);

        controls::FontIcon icon{};
        icon.Glyph(L"\uE8D6");
        icon.FontSize(13);
        icon.Width(16);
        icon.Foreground(BrushFor(m_palette.Text2));
        place(icon, 3);

        controls::StackPanel info{};
        info.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::TextBlock name{};
        name.Text(res::GetString(L"KeyAndChordsRowName"));
        name.FontSize(13);
        name.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        name.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        name.TextWrapping(xaml::TextWrapping::NoWrap);
        info.Children().Append(name);

        // The key at the playhead, and how many chords there are.
        winrt::hstring keyText = res::GetString(L"KeyAndChordsNoKey");
        size_t chordCount{ 0 };

        if (m_doc.Harmony)
        {
            chordCount = m_doc.Harmony->Chords.size();

            if (auto const key = seq::KeyAt(*m_doc.Harmony, m_position); key != nullptr)
            {
                auto const& scales = KeyScaleNames();
                auto const scaleIndex = static_cast<size_t>(key->Scale);
                keyText = winrt::hstring{ seq::NoteName(key->Root, seq::KeyUsesFlats(key->Root, key->Scale)) + L" " +
                    std::wstring{ scaleIndex < scales.size() ? scales[scaleIndex] : winrt::hstring{} } };
            }
        }

        controls::TextBlock sub{};
        sub.Text(keyText + L" \u00B7 " + res::FormatString(chordCount == 1 ? L"KeyAndChordsOneChord" : L"KeyAndChordsChordCountFormat", chordCount));
        sub.FontSize(11);
        sub.Margin(xaml::Thickness{ 0, 1, 0, 0 });
        sub.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        sub.TextWrapping(xaml::TextWrapping::NoWrap);
        sub.Foreground(BrushFor(m_palette.Text3));
        info.Children().Append(sub);

        place(info, 4);

        automation::AutomationProperties::SetName(grid, res::GetString(L"KeyAndChordsRowName"));
        return grid;
    }

    // ---------------------------------------------------------------- picking and dragging

    _Use_decl_annotations_
    MainWindow::HarmonyHit MainWindow::HitTestHarmony(seq::ArrangeRow const&, float top, foundation::Point point) const
    {
        HarmonyHit hit{};

        if (!m_doc.Harmony)
        {
            return hit;
        }

        auto const& harmony = *m_doc.Harmony;
        auto const pixelsPerTick = PixelsPerTick(m_barWidth);
        auto const x = static_cast<double>(point.X);
        auto const xAt = [this, pixelsPerTick](int64_t tick) { return static_cast<double>(tick) * pixelsPerTick - m_scrollX; };

        if (point.Y < top + static_cast<float>(seq::HarmonyKeyBandHeight))
        {
            hit.KeyBand = true;

            for (size_t i = harmony.Keys.size(); i-- > 0;)
            {
                if (x >= xAt(harmony.Keys[i].Tick) - 2.0)
                {
                    hit.Index = i;
                    break;
                }
            }

            return hit;
        }

        for (size_t i = 0; i < harmony.Chords.size(); ++i)
        {
            auto const& chord = harmony.Chords[i];
            auto const left = xAt(chord.Tick);
            auto const right = xAt(chord.Tick + chord.Length);

            if (x >= left && x < right)
            {
                hit.Index = i;
                hit.OnRightEdge = right - x <= HarmonyResizeEdge && right - left > HarmonyResizeEdge * 2;
                break;
            }
        }

        return hit;
    }

    _Use_decl_annotations_
    void MainWindow::OnHarmonyPressed(bool pinned, seq::ArrangeRow const& row, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const position = args.GetCurrentPoint(canvas).Position();
            auto const top = static_cast<float>(row.Top - (pinned ? 0.0 : m_scrollY));
            auto const hit = HitTestHarmony(row, top, position);

            ClearAutomationSelection();
            m_selectedPlacement = SIZE_MAX;
            m_selectedChord = SIZE_MAX;
            m_selectedKey = SIZE_MAX;

            if (hit.Index != SIZE_MAX && m_doc.Harmony)
            {
                HarmonyDrag drag{};
                drag.Pinned = pinned;
                drag.Index = hit.Index;
                drag.StartX = position.X;
                drag.Before = m_doc.Harmony;

                if (hit.KeyBand)
                {
                    m_selectedKey = hit.Index;
                    drag.Kind = HarmonyDragKind::MoveKey;
                    drag.OriginalTick = m_doc.Harmony->Keys[hit.Index].Tick;
                }
                else
                {
                    m_selectedChord = hit.Index;
                    auto const& chord = m_doc.Harmony->Chords[hit.Index];
                    drag.Kind = hit.OnRightEdge ? HarmonyDragKind::ResizeChord : HarmonyDragKind::MoveChord;
                    drag.OriginalTick = chord.Tick;
                    drag.OriginalLength = chord.Length;
                }

                m_harmonyDrag = std::move(drag);
                canvas.CapturePointer(args.Pointer());
            }

            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a click on the Key and chords row.")
    }

    _Use_decl_annotations_
    void MainWindow::OnHarmonyDragMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto& drag = m_harmonyDrag;

            if (!m_doc.Harmony)
            {
                drag = HarmonyDrag{};
                return;
            }

            auto const canvas = drag.Pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const dx = static_cast<double>(args.GetCurrentPoint(canvas).Position().X) - drag.StartX;

            if (!drag.Moved && std::abs(dx) < HarmonyDragThreshold)
            {
                return;
            }

            drag.Moved = true;

            auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);
            auto const ticks = static_cast<int64_t>(std::llround(dx / PixelsPerTick(m_barWidth)));
            auto& harmony = *m_doc.Harmony;

            switch (drag.Kind)
            {
            case HarmonyDragKind::MoveChord:
                if (drag.Index < harmony.Chords.size())
                {
                    harmony.Chords[drag.Index].Tick = std::max<int64_t>(0, seq::SnapTick(drag.OriginalTick + ticks, grid));
                }
                break;

            case HarmonyDragKind::ResizeChord:
                if (drag.Index < harmony.Chords.size())
                {
                    harmony.Chords[drag.Index].Length = std::max<int64_t>(grid, seq::SnapTick(drag.OriginalLength + ticks, grid));
                }
                break;

            case HarmonyDragKind::MoveKey:
                if (drag.Index < harmony.Keys.size())
                {
                    harmony.Keys[drag.Index].Tick = std::max<int64_t>(0, seq::SnapTick(drag.OriginalTick + ticks, grid));
                }
                break;

            default:
                break;
            }

            InvalidateArrange();
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to drag on the Key and chords row.")
    }

    void MainWindow::OnHarmonyDragReleased()
    {
        try
        {
            // Finished first: letting go of the capture comes straight back here.
            auto drag = std::exchange(m_harmonyDrag, HarmonyDrag{});

            if (!drag.Moved || !m_doc.Harmony || !drag.Before)
            {
                return;
            }

            auto after = *m_doc.Harmony;
            winrt::hstring undoName{};

            if (drag.Kind == HarmonyDragKind::MoveKey)
            {
                undoName = res::GetString(L"UndoMoveKey");
                auto const moved = drag.Index < after.Keys.size() ? std::optional<seq::KeyChange>{ after.Keys[drag.Index] } : std::nullopt;
                seq::NormalizeHarmony(after);

                if (moved)
                {
                    auto const found = std::find(after.Keys.begin(), after.Keys.end(), *moved);
                    m_selectedKey = found != after.Keys.end() ? static_cast<size_t>(found - after.Keys.begin()) : SIZE_MAX;
                }
            }
            else
            {
                undoName = res::GetString(drag.Kind == HarmonyDragKind::ResizeChord ? L"UndoResizeChord" : L"UndoMoveChord");

                // The chord goes back on the row as if newly placed, so whatever it now covers gives way.
                if (drag.Index < after.Chords.size())
                {
                    auto const chord = after.Chords[drag.Index];
                    after.Chords.erase(after.Chords.begin() + static_cast<ptrdiff_t>(drag.Index));
                    m_selectedChord = seq::PlaceChord(after, chord);
                }

                seq::NormalizeHarmony(after);
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeHarmonyChange(std::move(drag.Before), after));
            m_doc.Harmony = std::move(after);
            Commit(std::wstring{ undoName }, std::move(changes));
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish a drag on the Key and chords row.")
    }

    // ---------------------------------------------------------------- adding and deleting

    _Use_decl_annotations_
    void MainWindow::OnHarmonyDoubleTapped(bool pinned, seq::ArrangeRow const& row, foundation::Point point, int64_t tick)
    {
        try
        {
            auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
            auto const top = static_cast<float>(row.Top - (pinned ? 0.0 : m_scrollY));
            auto const hit = HitTestHarmony(row, top, point);

            if (hit.KeyBand)
            {
                // On a key: change it. Before the first key, or with none: add one.
                if (hit.Index != SIZE_MAX)
                {
                    m_selectedKey = hit.Index;
                    ShowKeyPicker(hit.Index, canvas, point);
                }
                else
                {
                    AddKeyAt(tick);
                }

                return;
            }

            if (hit.Index != SIZE_MAX)
            {
                m_selectedChord = hit.Index;
                InvalidateArrange();
                ShowChordPicker(hit.Index, canvas, point);
                return;
            }

            AddChordAt(tick);

            if (m_selectedChord != SIZE_MAX)
            {
                ShowChordPicker(m_selectedChord, canvas, point);
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a double click on the Key and chords row.")
    }

    _Use_decl_annotations_
    void MainWindow::AddChordAt(int64_t tick) noexcept
    {
        try
        {
            if (!m_doc.Harmony)
            {
                return;
            }

            auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);
            auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
            auto const bar = seq::TicksPerBar(meter);

            // From the bar it's in, a bar long, and on the key's root: a place to start from.
            seq::ChordEvent chord{};
            chord.Tick = std::max<int64_t>(0, (std::max<int64_t>(0, tick) / std::max<int64_t>(1, bar)) * bar);
            chord.Length = std::max<int64_t>(grid, bar);

            if (auto const key = seq::KeyAt(*m_doc.Harmony, chord.Tick); key != nullptr)
            {
                chord.Root = key->Root;
                chord.Type = seq::TonicChordType(key->Scale);
            }

            // Up to the next chord, never over it.
            for (auto const& other : m_doc.Harmony->Chords)
            {
                if (other.Tick > chord.Tick && other.Tick < chord.Tick + chord.Length)
                {
                    chord.Length = other.Tick - chord.Tick;
                }
            }

            size_t index{ SIZE_MAX };

            EditHarmony(res::GetString(L"UndoAddChord"), [&chord, &index](seq::HarmonyTrack& harmony)
            {
                index = seq::PlaceChord(harmony, chord);
            });

            if (m_doc.Harmony && index < m_doc.Harmony->Chords.size())
            {
                // Normalizing keeps the order, so the index is still the chord's.
                m_selectedChord = index;
                m_selectedKey = SIZE_MAX;
                InvalidateArrange();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a chord.")
    }

    _Use_decl_annotations_
    void MainWindow::AddKeyAt(int64_t tick) noexcept
    {
        try
        {
            if (!m_doc.Harmony)
            {
                return;
            }

            auto const grid = std::max<int64_t>(SnapGrid(), seq::TicksPerQuarterNote);
            seq::KeyChange key{};
            key.Tick = m_doc.Harmony->Keys.empty() ? 0 : std::max<int64_t>(0, seq::SnapTick(tick, grid));

            // A new key starts as the one before it, so changing it is one step.
            if (auto const before = seq::KeyAt(*m_doc.Harmony, key.Tick); before != nullptr)
            {
                key.Root = before->Root;
                key.Scale = before->Scale;
            }

            EditHarmony(res::GetString(L"UndoAddKey"), [&key](seq::HarmonyTrack& harmony)
            {
                harmony.Keys.push_back(key);
            });

            if (m_doc.Harmony)
            {
                auto const found = std::find(m_doc.Harmony->Keys.begin(), m_doc.Harmony->Keys.end(), key);
                m_selectedKey = found != m_doc.Harmony->Keys.end() ? static_cast<size_t>(found - m_doc.Harmony->Keys.begin()) : SIZE_MAX;
                m_selectedChord = SIZE_MAX;
                InvalidateArrange();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a key change.")
    }

    void MainWindow::DeleteSelectedHarmony() noexcept
    {
        try
        {
            if (!m_doc.Harmony)
            {
                return;
            }

            if (m_selectedChord < m_doc.Harmony->Chords.size())
            {
                auto const index = std::exchange(m_selectedChord, SIZE_MAX);
                EditHarmony(res::GetString(L"UndoDeleteChord"), [index](seq::HarmonyTrack& harmony)
                {
                    harmony.Chords.erase(harmony.Chords.begin() + static_cast<ptrdiff_t>(index));
                });
            }
            else if (m_selectedKey < m_doc.Harmony->Keys.size())
            {
                auto const index = std::exchange(m_selectedKey, SIZE_MAX);
                EditHarmony(res::GetString(L"UndoDeleteKey"), [index](seq::HarmonyTrack& harmony)
                {
                    harmony.Keys.erase(harmony.Keys.begin() + static_cast<ptrdiff_t>(index));
                });
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to delete from the Key and chords row.")
    }

    // ---------------------------------------------------------------- the menu

    _Use_decl_annotations_
    void MainWindow::AddHarmonyMenuItems(controls::MenuFlyout const& menu, seq::ArrangeRow const& row, bool pinned, foundation::Point point, int64_t tick)
    {
        auto weak = get_weak();
        auto const canvas = pinned ? m_pinnedLaneCanvas : m_scrollLaneCanvas;
        auto const top = static_cast<float>(row.Top - (pinned ? 0.0 : m_scrollY));
        auto const hit = HitTestHarmony(row, top, point);

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        if (hit.KeyBand && hit.Index != SIZE_MAX)
        {
            m_selectedKey = hit.Index;
            m_selectedChord = SIZE_MAX;
            auto const index = hit.Index;
            add(res::GetString(L"MenuEditKey"), L"\uE70F", [weak, index, canvas, point]() { if (auto s = weak.get()) { s->ShowKeyPicker(index, canvas, point); } });
            add(res::GetString(L"MenuDeleteKey"), L"\uE74D", [weak]() { if (auto s = weak.get()) { s->DeleteSelectedHarmony(); } });
        }
        else if (!hit.KeyBand && hit.Index != SIZE_MAX)
        {
            m_selectedChord = hit.Index;
            m_selectedKey = SIZE_MAX;
            auto const index = hit.Index;
            add(res::GetString(L"MenuEditChord"), L"\uE70F", [weak, index, canvas, point]() { if (auto s = weak.get()) { s->ShowChordPicker(index, canvas, point); } });
            add(res::GetString(L"MenuDeleteChord"), L"\uE74D", [weak]() { if (auto s = weak.get()) { s->DeleteSelectedHarmony(); } });
        }
        else
        {
            add(res::GetString(L"MenuAddChord"), L"\uE710", [weak, tick]() { if (auto s = weak.get()) { s->AddChordAt(tick); } });
        }

        add(res::GetString(L"MenuAddKey"), L"\uE710", [weak, tick]() { if (auto s = weak.get()) { s->AddKeyAt(tick); } });

        menu.Items().Append(controls::MenuFlyoutSeparator{});

        controls::ToggleMenuFlyoutItem numerals{};
        numerals.Text(res::GetString(L"MenuRomanNumerals"));
        numerals.IsChecked(seq::AppSettings::Current().ShowRomanNumerals());
        numerals.Click([weak](foundation::IInspectable const& sender, auto&&)
        {
            if (auto s = weak.get())
            {
                seq::AppSettings::Current().ShowRomanNumerals(sender.as<controls::ToggleMenuFlyoutItem>().IsChecked());
                s->InvalidateArrange();
            }
        });
        menu.Items().Append(numerals);

        add(res::GetString(L"MenuHideKeyAndChords"), L"\uED1A", [weak]() { if (auto s = weak.get()) { s->HideHarmonyRow(); } });
        menu.Items().Append(controls::MenuFlyoutSeparator{});

        InvalidateArrange();
    }

    // ---------------------------------------------------------------- the pickers

    _Use_decl_annotations_
    void MainWindow::ShowChordPicker(size_t chordIndex, xaml::UIElement const& anchor, foundation::Point point)
    {
        if (!m_doc.Harmony || chordIndex >= m_doc.Harmony->Chords.size())
        {
            return;
        }

        auto weak = get_weak();
        auto const tick = m_doc.Harmony->Chords[chordIndex].Tick;

        // Each change finds the chord again by where it starts, since an undo or another edit can
        // move it in the list.
        auto const current = [weak, tick]() -> std::optional<seq::ChordEvent>
        {
            if (auto s = weak.get(); s && s->m_doc.Harmony)
            {
                for (auto const& chord : s->m_doc.Harmony->Chords)
                {
                    if (chord.Tick == tick)
                    {
                        return chord;
                    }
                }
            }

            return std::nullopt;
        };

        auto const keyFlats = [weak, tick]()
        {
            if (auto s = weak.get(); s && s->m_doc.Harmony)
            {
                if (auto const key = seq::KeyAt(*s->m_doc.Harmony, tick); key != nullptr)
                {
                    return seq::KeyUsesFlats(key->Root, key->Scale);
                }
            }

            return false;
        };

        controls::StackPanel panel{};
        panel.Spacing(8);
        panel.MinWidth(320);

        auto const heading = [&panel](winrt::hstring const& text)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(12);
            block.Opacity(0.8);
            panel.Children().Append(block);
        };

        auto const smallButton = [](winrt::hstring const& text)
        {
            controls::Primitives::ToggleButton button{};
            button.Content(winrt::box_value(text));
            button.MinWidth(36);
            button.Padding(xaml::Thickness{ 6, 3, 6, 3 });
            button.FontSize(12);
            return button;
        };

        auto roots = std::make_shared<std::vector<controls::Primitives::ToggleButton>>();
        auto types = std::make_shared<std::vector<std::pair<seq::ChordType, controls::Primitives::ToggleButton>>>();
        controls::ComboBox bass{};
        controls::DropDownButton more{};

        // Every control shows the chord as it is now, named in its key.
        auto refresh = std::make_shared<std::function<void()>>();
        *refresh = [current, keyFlats, roots, types, bass, more]()
        {
            auto const chord = current();

            if (!chord)
            {
                return;
            }

            for (uint8_t i = 0; i < roots->size(); ++i)
            {
                (*roots)[i].IsChecked(chord->Root == i);
            }

            for (auto const& [type, button] : *types)
            {
                button.Content(winrt::box_value(winrt::hstring{ seq::NoteName(chord->Root, keyFlats()) + seq::ChordTypeSuffix(type) }));
                button.IsChecked(chord->Type == type);
            }

            auto const isMore = std::find(std::begin(MoreTypes), std::end(MoreTypes), chord->Type) != std::end(MoreTypes);
            more.Content(winrt::box_value(isMore ? winrt::hstring{ seq::NoteName(chord->Root, keyFlats()) + seq::ChordTypeSuffix(chord->Type) } : res::GetString(L"ChordPickerMore")));

            bass.SelectedIndex(chord->Bass ? static_cast<int32_t>(*chord->Bass) + 1 : 0);
        };

        auto const change = [weak, tick, refresh](std::function<void(seq::ChordEvent&)> edit)
        {
            if (auto s = weak.get())
            {
                s->EditHarmony(res::GetString(L"UndoEditChord"), [tick, &edit](seq::HarmonyTrack& harmony)
                {
                    for (auto& chord : harmony.Chords)
                    {
                        if (chord.Tick == tick)
                        {
                            edit(chord);
                            break;
                        }
                    }
                });

                (*refresh)();
            }
        };

        // Root: all 12, spelled to suit the key.
        heading(res::GetString(L"ChordPickerRoot"));
        controls::VariableSizedWrapGrid rootGrid{};
        rootGrid.Orientation(controls::Orientation::Horizontal);
        rootGrid.MaximumRowsOrColumns(6);

        for (uint8_t i = 0; i < 12; ++i)
        {
            auto button = smallButton(winrt::hstring{ seq::NoteName(i, keyFlats()) });
            button.Margin(xaml::Thickness{ 0, 0, 4, 4 });
            button.Click([change, i](auto&&, auto&&) { change([i](seq::ChordEvent& chord) { chord.Root = i; }); });
            roots->push_back(button);
            rootGrid.Children().Append(button);
        }

        panel.Children().Append(rootGrid);

        // Type: the usual ones as buttons, the rest under More.
        heading(res::GetString(L"ChordPickerType"));
        controls::VariableSizedWrapGrid typeGrid{};
        typeGrid.Orientation(controls::Orientation::Horizontal);
        typeGrid.MaximumRowsOrColumns(4);

        for (auto const type : MainTypes)
        {
            auto button = smallButton(winrt::hstring{});
            button.MinWidth(64);
            button.Margin(xaml::Thickness{ 0, 0, 4, 4 });
            button.Click([change, type](auto&&, auto&&) { change([type](seq::ChordEvent& chord) { chord.Type = type; }); });
            types->emplace_back(type, button);
            typeGrid.Children().Append(button);
        }

        panel.Children().Append(typeGrid);

        controls::MenuFlyout moreMenu{};
        auto const rootAtOpen = m_doc.Harmony->Chords[chordIndex].Root;

        for (auto const type : MoreTypes)
        {
            controls::MenuFlyoutItem item{};
            item.Text(winrt::hstring{ seq::NoteName(rootAtOpen, keyFlats()) + seq::ChordTypeSuffix(type) });
            item.Click([change, type](auto&&, auto&&) { change([type](seq::ChordEvent& chord) { chord.Type = type; }); });
            moreMenu.Items().Append(item);
        }

        more.Flyout(moreMenu);
        more.FontSize(12);
        automation::AutomationProperties::SetName(more, res::GetString(L"ChordPickerMore"));
        panel.Children().Append(more);

        // Bass: the root, or any note, for chords like C/E.
        bass.Header(winrt::box_value(res::GetString(L"ChordPickerBass")));
        bass.MinWidth(140);
        bass.Items().Append(winrt::box_value(res::GetString(L"ChordBassRoot")));

        for (uint8_t i = 0; i < 12; ++i)
        {
            bass.Items().Append(winrt::box_value(winrt::hstring{ seq::NoteName(i, keyFlats()) }));
        }

        panel.Children().Append(bass);

        (*refresh)();

        bass.SelectionChanged([change, current](foundation::IInspectable const& sender, auto&&)
        {
            auto const index = sender.as<controls::ComboBox>().SelectedIndex();
            auto const chord = current();

            if (index < 0 || !chord)
            {
                return;
            }

            std::optional<uint8_t> wanted{};

            if (index > 0)
            {
                wanted = static_cast<uint8_t>(index - 1);
            }

            if (wanted == chord->Root)
            {
                wanted.reset();
            }

            if (wanted != chord->Bass)
            {
                change([wanted](seq::ChordEvent& edited) { edited.Bass = wanted; });
            }
        });

        controls::Flyout flyout{};
        flyout.Content(panel);

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        flyout.ShowAt(anchor, options);
    }

    _Use_decl_annotations_
    void MainWindow::ShowKeyPicker(size_t keyIndex, xaml::UIElement const& anchor, foundation::Point point)
    {
        if (!m_doc.Harmony || keyIndex >= m_doc.Harmony->Keys.size())
        {
            return;
        }

        auto weak = get_weak();
        auto const tick = m_doc.Harmony->Keys[keyIndex].Tick;
        auto const original = m_doc.Harmony->Keys[keyIndex];

        controls::StackPanel panel{};
        panel.Spacing(10);
        panel.MinWidth(240);

        controls::ComboBox root{};
        root.Header(winrt::box_value(res::GetString(L"KeyPickerRoot")));
        root.MinWidth(200);

        for (uint8_t i = 0; i < 12; ++i)
        {
            root.Items().Append(winrt::box_value(winrt::hstring{ seq::NoteName(i, seq::KeyUsesFlats(i, original.Scale)) }));
        }

        root.SelectedIndex(original.Root);
        panel.Children().Append(root);

        controls::ComboBox scale{};
        scale.Header(winrt::box_value(res::GetString(L"KeyPickerScale")));
        scale.MinWidth(200);

        for (auto const key : ScaleNameKeys)
        {
            scale.Items().Append(winrt::box_value(res::GetString(key)));
        }

        scale.SelectedIndex(static_cast<int32_t>(original.Scale));
        panel.Children().Append(scale);

        controls::ToggleSwitch transpose{};
        transpose.Header(winrt::box_value(res::GetString(L"KeyPickerTranspose")));
        automation::AutomationProperties::SetName(transpose, res::GetString(L"KeyPickerTranspose"));
        automation::AutomationProperties::SetHelpText(transpose, res::GetString(L"KeyPickerTransposeHelp"));
        transpose.IsOn(false);
        panel.Children().Append(transpose);

        controls::TextBlock help{};
        help.Text(res::GetString(L"KeyPickerTransposeHelp"));
        help.TextWrapping(xaml::TextWrapping::Wrap);
        help.FontSize(12);
        help.MaxWidth(260);
        help.Foreground(BrushFor(m_palette.Text2));
        panel.Children().Append(help);

        // The key that starts at this tick, changed, and the chords up to the next key moved with
        // it when Transpose chords is on.
        auto const apply = [weak, tick, root, scale, transpose]()
        {
            auto s = weak.get();

            if (!s || root.SelectedIndex() < 0 || scale.SelectedIndex() < 0)
            {
                return;
            }

            auto const newRoot = static_cast<uint8_t>(root.SelectedIndex());
            auto const newScale = static_cast<seq::ScaleKind>(scale.SelectedIndex());
            auto const moveChords = transpose.IsOn();

            s->EditHarmony(res::GetString(L"UndoEditKey"), [tick, newRoot, newScale, moveChords](seq::HarmonyTrack& harmony)
            {
                for (size_t i = 0; i < harmony.Keys.size(); ++i)
                {
                    auto& key = harmony.Keys[i];

                    if (key.Tick != tick)
                    {
                        continue;
                    }

                    if (moveChords && key.Root != newRoot)
                    {
                        auto const until = i + 1 < harmony.Keys.size() ? harmony.Keys[i + 1].Tick : INT64_MAX;
                        auto semitones = static_cast<int>(newRoot) - static_cast<int>(key.Root);

                        // The nearer way round, so chords don't jump more than half an octave.
                        if (semitones > 6) { semitones -= 12; }
                        if (semitones < -6) { semitones += 12; }

                        seq::TransposeChords(harmony, tick, until, semitones);
                    }

                    key.Root = newRoot;
                    key.Scale = newScale;
                    break;
                }
            });
        };

        root.SelectionChanged([apply](auto&&, auto&&) { apply(); });
        scale.SelectionChanged([apply](auto&&, auto&&) { apply(); });

        controls::Flyout flyout{};
        flyout.Content(panel);

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        flyout.ShowAt(anchor, options);
    }
}
