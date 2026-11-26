// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Chords and scales: a chord's notes, the name of a set of notes, and the chords in a key. Laid
// out as src\prototypes\midi-calculator\design\3-chords-and-scales.html. The music comes from
// midi-app-shared\ChordTheory.h, which the Windows MIDI Sequencer uses too, so a chord is spelled,
// read and voiced the same way in both apps.

#include "pch.h"
#include "ThemeBrushes.h"
#include "MainWindow.xaml.h"

#include "CalcText.h"
#include "StringResources.h"

namespace native = ::midicalculator;
namespace res = ::midicalculator::resources;
namespace ui = ::midicalculator::ui;
namespace theory = ::midiapp;

namespace winrt::midicalculator::implementation
{
    namespace
    {
        // the names the lists show; a key's own spelling is used everywhere else
        constexpr size_t MaximumNames = 6;

        media::Brush Brush(wchar_t const* key)
        {
            return ::midiapp::ThemeBrushes::Current().Get(key);
        }

        media::SolidColorBrush BassBrush()
        {
            return media::SolidColorBrush{ winrt::Windows::UI::Color{ 0xFF, 0xFF, 0xCF, 0x8A } };
        }

        bool HasMinorThird(theory::ChordType type)
        {
            auto const intervals = theory::ChordIntervals(type);
            auto const has = [&](uint8_t interval) { return std::find(intervals.begin(), intervals.end(), interval) != intervals.end(); };
            return has(3) && !has(4);
        }

        // The key a chord's own notes are spelled in: D minor for Dm7, so its third is F and not E♯.
        theory::MusicKey SpellingKey(theory::ChordSymbol const& chord)
        {
            return { chord.Root, HasMinorThird(chord.Type) ? theory::ScaleKind::NaturalMinor : theory::ScaleKind::Major };
        }

        // "D3": the note's name, spelled in the key, with its octave, middle C (60) in the octave
        // the customer chose.
        std::wstring NoteWithOctave(uint8_t note, theory::MusicKey const* key)
        {
            auto const octave = static_cast<int32_t>(note / 12) - (5 - static_cast<int32_t>(native::AppSettings::Current().MiddleCOctave()));
            return theory::NoteNameInKey(static_cast<uint8_t>(note % 12), key) + std::to_wstring(octave);
        }

        // What a note is in a chord, the way a chart writes it: 3, ♭3, 5, ♭7, 9...
        std::wstring_view IntervalName(theory::ChordType type, uint8_t interval)
        {
            if (type == theory::ChordType::Diminished7 && interval == 9)
            {
                return L"\u266D\u266D7";
            }

            switch (interval)
            {
            case 1: return L"\u266D2";
            case 2: return L"2";
            case 3: return L"\u266D3";
            case 4: return L"3";
            case 5: return L"4";
            case 6: return L"\u266D5";
            case 7: return L"5";
            case 8: return L"\u266F5";
            case 9: return L"6";
            case 10: return L"\u266D7";
            case 11: return L"7";
            case 14: return L"9";
            case 17: return L"11";
            case 21: return L"13";
            default: return L"";
            }
        }

        // The notes of a chord as names, in the order they're stacked: "A C E G".
        std::wstring PitchClassList(theory::ChordSymbol const& chord)
        {
            auto const key = SpellingKey(chord);
            std::wstring list{};

            for (auto const pitchClass : theory::ChordPitchClasses(chord))
            {
                if (!list.empty())
                {
                    list += L' ';
                }

                list += theory::NoteNameInKey(pitchClass, &key);
            }

            return list;
        }

        controls::TextBlock SmallText(winrt::hstring const& text)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(11);
            block.Foreground(Brush(L"TextFillColorTertiaryBrush"));
            block.HorizontalAlignment(xaml::HorizontalAlignment::Center);
            return block;
        }

        // A labeled group of controls under one header, like the comp's "Or pick".
        xaml::FrameworkElement HeaderOver(winrt::hstring const& label, xaml::FrameworkElement const& content)
        {
            controls::StackPanel panel{};
            panel.Spacing(8);
            panel.Children().Append(ui::MakeFieldHeader(label));
            panel.Children().Append(content);
            return panel;
        }

        winrt::hstring ScaleName(theory::ScaleKind scale)
        {
            return res::GetString(std::format(L"ScaleName{}", static_cast<int32_t>(scale)));
        }
    }

    void MainWindow::BuildChordsPage()
    {
        auto const page = res::GetString(L"PageChords");
        auto columns = std::make_shared<ui::CardColumns>();

        auto const namer = [](uint8_t note, bool label) -> winrt::hstring
            {
                auto const name = NoteWithOctave(note, nullptr);
                return label ? winrt::hstring{ name } : res::FormatString(L"ChordKeyName", name, note);
            };

        // ---- chord to notes (left) ----
        {
            auto card = ui::MakeCard(res::GetString(L"ChordToNotesTitle"), L"\uEC4F");

            m_chordText = ui::MakeTextBox(res::GetString(L"ChordLabel"), L"Dm7/G", 180);

            std::vector<winrt::hstring> roots{};
            std::vector<winrt::hstring> basses{ res::GetString(L"ChordBassNone") };

            for (uint8_t pitchClass = 0; pitchClass < 12; ++pitchClass)
            {
                auto const name = theory::NoteNameInKey(pitchClass, nullptr);
                roots.push_back(winrt::hstring{ name });
                basses.push_back(winrt::hstring{ L"/ " + name });
            }

            std::vector<winrt::hstring> types{};

            for (size_t i = 0; i < theory::ChordTypeCount; ++i)
            {
                auto const suffix = theory::ChordTypeSuffix(static_cast<theory::ChordType>(i));
                types.push_back(suffix.empty() ? res::GetString(L"ChordTypeMajor") : winrt::hstring{ suffix });
            }

            m_chordRoot = ui::MakeComboBox(res::GetString(L"ChordRootName"), roots, 2, 70);
            m_chordType = ui::MakeComboBox(res::GetString(L"ChordTypeName"), types, static_cast<int32_t>(theory::ChordType::Minor7), 100);
            m_chordBass = ui::MakeComboBox(res::GetString(L"ChordBassName"), basses, 8, 110);

            // one header over the three lists, as in the comp
            controls::StackPanel pickers{};
            pickers.Orientation(controls::Orientation::Horizontal);
            pickers.Spacing(6);

            for (auto const& box : { m_chordRoot, m_chordType, m_chordBass })
            {
                box.Header(nullptr);
                pickers.Children().Append(box);
            }

            card.Body.Children().Append(m_chordText);
            card.Body.Children().Append(HeaderOver(res::GetString(L"ChordPickLabel"), pickers));

            // three octaves, so a chord's top notes, up to a ninth above the root, are on it
            m_chordKeys = std::make_shared<ui::PianoKeyboard>(res::GetString(L"ChordKeysName"), namer, nullptr, 3);
            card.Body.Children().Append(m_chordKeys->Root());

            m_chordNotes = controls::StackPanel{};
            m_chordNotes.Orientation(controls::Orientation::Horizontal);
            m_chordNotes.Spacing(6);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_chordNotes, res::GetString(L"ChordNotesName"));
            card.Body.Children().Append(m_chordNotes);

            m_chordProblem = ui::MakeHelp(L"");
            m_chordProblem.Visibility(xaml::Visibility::Collapsed);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetLiveSetting(m_chordProblem, winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationLiveSetting::Polite);
            card.Body.Children().Append(m_chordProblem);

            m_chordCopy = ui::MakeSmallButton(res::GetString(L"ChordCopyNotes"), L"\uE8C8", false, false);
            m_chordCopy.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            m_chordCopy.Click([weak = get_weak(), page](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && s->m_chord.has_value())
                    {
                        auto const key = SpellingKey(*s->m_chord);
                        std::wstring notes{};

                        for (auto const note : theory::ChordNotes(*s->m_chord))
                        {
                            notes += (notes.empty() ? L"" : L" ") + NoteWithOctave(note, &key);
                        }

                        s->CopyAnswer(page, res::FormatString(L"ChordNotesOf", theory::ChordName(*s->m_chord, nullptr)), winrt::hstring{ notes });
                    }
                });
            card.Body.Children().Append(m_chordCopy);

            m_chordText.TextChanged([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get(); s && !s->m_suppressChord)
                    {
                        s->UpdateChordToNotes();
                    }
                });

            auto picked = [weak = get_weak()](auto&&, auto&&)
                {
                    auto s = weak.get();

                    if (!s || s->m_suppressChord || s->m_chordRoot.SelectedIndex() < 0 || s->m_chordType.SelectedIndex() < 0)
                    {
                        return;
                    }

                    theory::ChordSymbol chord{};
                    chord.Root = static_cast<uint8_t>(s->m_chordRoot.SelectedIndex());
                    chord.Type = static_cast<theory::ChordType>(s->m_chordType.SelectedIndex());

                    if (auto const bass = s->m_chordBass.SelectedIndex(); bass > 0 && bass - 1 != chord.Root)
                    {
                        chord.Bass = static_cast<uint8_t>(bass - 1);
                    }

                    s->m_suppressChord = true;
                    s->m_chordText.Text(theory::ChordName(chord, nullptr));
                    s->m_suppressChord = false;
                    s->UpdateChordToNotes();
                };

            m_chordRoot.SelectionChanged(picked);
            m_chordType.SelectionChanged(picked);
            m_chordBass.SelectionChanged(picked);

            columns->Add(card, 0);
        }

        // ---- name a chord (right) ----
        {
            auto card = ui::MakeCard(res::GetString(L"NameChordTitle"), L"\uE8D6");

            // the help, with Clear at the end of its line
            controls::Grid helpRow{};
            helpRow.ColumnSpacing(10);
            {
                controls::ColumnDefinition words{};
                words.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                controls::ColumnDefinition button{};
                button.Width(xaml::GridLengthHelper::Auto());
                helpRow.ColumnDefinitions().Append(words);
                helpRow.ColumnDefinitions().Append(button);
            }

            auto help = ui::MakeHelp(res::GetString(L"NameChordHelp"));
            help.VerticalAlignment(xaml::VerticalAlignment::Center);
            helpRow.Children().Append(help);

            auto clear = ui::MakeSmallButton(res::GetString(L"NameChordClear"), L"\uE894", false, true);
            clear.VerticalAlignment(xaml::VerticalAlignment::Center);
            clear.Click([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->m_heldNotes.clear();
                        s->UpdateNamedChord();
                    }
                });
            controls::Grid::SetColumn(clear, 1);
            helpRow.Children().Append(clear);
            card.Body.Children().Append(helpRow);

            m_nameKeys = std::make_shared<ui::PianoKeyboard>(res::GetString(L"NameChordKeysName"), namer,
                [weak = get_weak()](uint8_t note, bool on)
                {
                    if (auto s = weak.get())
                    {
                        auto& held = s->m_heldNotes;
                        held.erase(std::remove(held.begin(), held.end(), note), held.end());

                        if (on)
                        {
                            held.push_back(note);
                        }

                        s->UpdateNamedChord();
                    }
                });
            card.Body.Children().Append(m_nameKeys->Root());

            m_nameResults = controls::StackPanel{};
            m_nameResults.Spacing(6);
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_nameResults, res::GetString(L"NameChordResultsName"));
            card.Body.Children().Append(m_nameResults);

            // the comp's example: C E G A with C lowest
            m_heldNotes = { 60, 64, 67, 69 };

            columns->Add(card, 1);
        }

        // ---- chords in a key (both columns) ----
        {
            auto card = ui::MakeCard(res::GetString(L"KeyChordsTitle"), L"\uE8FD");

            std::vector<winrt::hstring> roots{};

            for (uint8_t pitchClass = 0; pitchClass < 12; ++pitchClass)
            {
                roots.push_back(winrt::hstring{ theory::NoteNameInKey(pitchClass, nullptr) });
            }

            std::vector<winrt::hstring> scales{};

            for (size_t i = 0; i < theory::ScaleKindCount; ++i)
            {
                scales.push_back(ScaleName(static_cast<theory::ScaleKind>(i)));
            }

            m_keyRoot = ui::MakeComboBox(res::GetString(L"KeyRootLabel"), roots, 9, 80);
            m_keyScale = ui::MakeComboBox(res::GetString(L"KeyScaleLabel"), scales, static_cast<int32_t>(theory::ScaleKind::NaturalMinor), 180);
            m_keySevenths = std::make_shared<ui::Segmented>(
                std::vector<winrt::hstring>{ res::GetString(L"KeyTriads"), res::GetString(L"KeySevenths") }, 0, res::GetString(L"KeyChordsLabel"));

            for (auto const& child : m_keySevenths->Root().Children())
            {
                child.as<xaml::FrameworkElement>().MinHeight(32);
            }

            m_keyNotes = controls::TextBlock{};
            m_keyNotes.FontSize(14);
            m_keyNotes.IsTextSelectionEnabled(true);
            auto notes = ui::MakeInlineText(m_keyNotes, true);
            notes.MinWidth(260);

            controls::Grid fields{};
            {
                controls::ColumnDefinition left{};
                left.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                controls::ColumnDefinition right{};
                right.Width(xaml::GridLengthHelper::Auto());
                fields.ColumnDefinitions().Append(left);
                fields.ColumnDefinitions().Append(right);
            }

            auto const scaleNotes = HeaderOver(res::GetString(L"KeyScaleNotesLabel"), notes);
            scaleNotes.VerticalAlignment(xaml::VerticalAlignment::Bottom);
            controls::Grid::SetColumn(scaleNotes, 1);
            fields.Children().Append(ui::MakeFieldRow({ m_keyRoot, m_keyScale, HeaderOver(res::GetString(L"KeyChordsLabel"), m_keySevenths->Root()) }));
            fields.Children().Append(scaleNotes);
            card.Body.Children().Append(fields);

            m_keyChords = controls::Grid{};
            m_keyChords.ColumnSpacing(6);

            for (int32_t i = 0; i < 7; ++i)
            {
                controls::ColumnDefinition column{};
                column.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                m_keyChords.ColumnDefinitions().Append(column);
            }

            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(m_keyChords, res::GetString(L"KeyChordsTitle"));
            card.Body.Children().Append(m_keyChords);
            card.Body.Children().Append(ui::MakeHelp(res::GetString(L"KeyChordsHelp")));

            auto update = [weak = get_weak()](auto&&, auto&&) { if (auto s = weak.get()) s->UpdateKeyChords(); };
            m_keyRoot.SelectionChanged(update);
            m_keyScale.SelectionChanged(update);
            m_keySevenths->Changed([weak = get_weak()](int32_t) { if (auto s = weak.get()) s->UpdateKeyChords(); });

            columns->Add(card, 0, true);
        }

        ChordsPage().Children().Append(columns->Root());
        m_columns.push_back(columns);

        UpdateChordsPage();
    }

    void MainWindow::UpdateChordsPage() noexcept
    {
        if (!m_chordKeys)
        {
            return;
        }

        try
        {
            // octave numbers follow the middle C setting
            m_chordKeys->Rename();
            m_nameKeys->Rename();
        }
        CATCH_LOG()

        UpdateChordToNotes();
        UpdateNamedChord();
        UpdateKeyChords();
    }

    _Use_decl_annotations_
    void MainWindow::ShowChordInPickers(theory::ChordSymbol const& chord) noexcept
    {
        try
        {
            m_suppressChord = true;
            m_chordRoot.SelectedIndex(chord.Root % 12);
            m_chordType.SelectedIndex(static_cast<int32_t>(chord.Type));
            m_chordBass.SelectedIndex(chord.Bass.has_value() ? *chord.Bass % 12 + 1 : 0);
            m_suppressChord = false;
        }
        catch (...)
        {
            m_suppressChord = false;
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void MainWindow::UpdateChordToNotes() noexcept
    {
        try
        {
            if (!m_chordText)
            {
                return;
            }

            auto const parsed = theory::ParseChordName(m_chordText.Text());
            m_chordNotes.Children().Clear();

            if (!parsed.has_value() || parsed->NoChord)
            {
                m_chord.reset();
                m_chordKeys->Show({}, std::nullopt);
                m_chordProblem.Text(res::GetString(parsed.has_value() ? L"ChordNoChord" : L"ChordNotUnderstood"));
                m_chordProblem.Visibility(xaml::Visibility::Visible);
                m_chordCopy.IsEnabled(false);
                HighlightKeyChord();
                return;
            }

            auto const& chord = parsed->Chord;
            m_chord = chord;
            m_chordProblem.Visibility(xaml::Visibility::Collapsed);
            m_chordCopy.IsEnabled(true);
            ShowChordInPickers(chord);

            auto const notes = theory::ChordNotes(chord);
            auto const key = SpellingKey(chord);
            auto const intervals = theory::ChordIntervals(chord.Type);
            auto const rootNote = static_cast<uint8_t>(60 + chord.Root % 12);
            auto const bassNote = notes.front();

            // the bass is lit orange; it's the chord's root an octave down unless the chord has
            // its own bass
            m_chordKeys->Show(notes, bassNote);

            for (auto const note : notes)
            {
                auto const isBass = note == bassNote;
                auto const isRoot = note == rootNote;

                winrt::hstring role{};

                if (isBass)
                {
                    role = res::GetString(L"ChordRoleBass");
                }
                else if (isRoot)
                {
                    role = res::GetString(L"ChordRoleRoot");
                }
                else
                {
                    auto const interval = static_cast<uint8_t>(note - rootNote);
                    role = std::find(intervals.begin(), intervals.end(), interval) != intervals.end()
                        ? winrt::hstring{ IntervalName(chord.Type, interval) }
                        : winrt::hstring{};
                }

                controls::Border chip{};
                chip.MinWidth(52);
                chip.Padding(xaml::Thickness{ 8, 6, 8, 6 });
                chip.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
                chip.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                chip.Background(Brush(L"ControlFillColorDefaultBrush"));
                chip.BorderBrush(isBass ? media::Brush{ BassBrush() } : isRoot ? Brush(L"AccentFillColorDefaultBrush") : Brush(L"CardStrokeColorDefaultBrush"));

                controls::StackPanel words{};
                words.Spacing(2);

                controls::TextBlock name{};
                name.Text(NoteWithOctave(note, &key));
                name.FontSize(16);
                name.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                name.HorizontalAlignment(xaml::HorizontalAlignment::Center);

                words.Children().Append(name);
                words.Children().Append(SmallText(res::FormatString(L"ChordRoleFormat", std::wstring_view{ role }, note)));
                chip.Child(words);

                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(chip,
                    res::FormatString(L"ChordNoteName", name.Text(), std::wstring_view{ role }, note));

                m_chordNotes.Children().Append(chip);
            }

            HighlightKeyChord();
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to show the chord's notes.")
    }

    void MainWindow::UpdateNamedChord() noexcept
    {
        try
        {
            if (!m_nameKeys)
            {
                return;
            }

            std::optional<uint8_t> bass{};

            if (m_heldNotes.size() >= 2)
            {
                bass = *std::min_element(m_heldNotes.begin(), m_heldNotes.end());
            }

            m_nameKeys->Show(m_heldNotes, bass);
            m_nameResults.Children().Clear();

            auto const names = theory::NameChords(m_heldNotes);

            if (names.empty())
            {
                controls::TextBlock none{};
                none.Text(res::GetString(m_heldNotes.size() < 2 ? L"NameChordPickNotes" : L"NameChordNone"));
                none.FontSize(13);
                none.Foreground(Brush(L"TextFillColorSecondaryBrush"));
                none.TextWrapping(xaml::TextWrapping::Wrap);
                m_nameResults.Children().Append(none);
                return;
            }

            auto const page = res::GetString(L"PageChords");

            for (size_t i = 0; i < names.size() && i < MaximumNames; ++i)
            {
                auto const& chord = names[i];
                winrt::hstring const name{ theory::ChordName(chord, nullptr) };
                auto const key = SpellingKey(chord);

                auto const detail = chord.Bass.has_value()
                    ? res::FormatString(L"NameChordOverBass", PitchClassList(chord), theory::NoteNameInKey(*chord.Bass, &key))
                    : res::FormatString(L"NameChordRootLowest", PitchClassList(chord));

                controls::Border entry{};
                entry.Padding(xaml::Thickness{ 12, 6, 4, 6 });
                entry.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
                entry.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
                entry.Background(Brush(L"ControlFillColorDefaultBrush"));
                entry.BorderBrush(i == 0 ? Brush(L"AccentFillColorDefaultBrush") : Brush(L"CardStrokeColorDefaultBrush"));

                controls::Grid grid{};
                grid.ColumnSpacing(10);

                {
                    controls::ColumnDefinition nameColumn{};
                    nameColumn.Width(xaml::GridLengthHelper::Auto());
                    nameColumn.MinWidth(70);
                    controls::ColumnDefinition detailColumn{};
                    detailColumn.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
                    controls::ColumnDefinition copyColumn{};
                    copyColumn.Width(xaml::GridLengthHelper::Auto());
                    grid.ColumnDefinitions().Append(nameColumn);
                    grid.ColumnDefinitions().Append(detailColumn);
                    grid.ColumnDefinitions().Append(copyColumn);
                }

                controls::TextBlock big{};
                big.Text(name);
                big.FontSize(18);
                big.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                big.VerticalAlignment(xaml::VerticalAlignment::Center);
                big.IsTextSelectionEnabled(true);

                controls::TextBlock small{};
                small.Text(detail);
                small.FontSize(12);
                small.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                small.VerticalAlignment(xaml::VerticalAlignment::Center);
                small.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
                controls::Grid::SetColumn(small, 1);

                controls::Button copy{};
                copy.Background(media::SolidColorBrush{ winrt::Microsoft::UI::Colors::Transparent() });
                copy.BorderThickness(xaml::Thickness{ 0 });
                copy.Padding(xaml::Thickness{ 8, 6, 8, 6 });
                copy.VerticalAlignment(xaml::VerticalAlignment::Center);
                {
                    controls::FontIcon icon{};
                    icon.Glyph(L"\uE8C8");
                    icon.FontSize(12);
                    icon.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                    copy.Content(icon);
                }
                auto const copyName = res::FormatString(L"CopyNameFormat", std::wstring_view{ name });
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(copy, copyName);
                controls::ToolTipService::SetToolTip(copy, winrt::box_value(copyName));

                auto const question = res::FormatString(L"NameChordQuestion", PitchClassList(chord));
                copy.Click([weak = get_weak(), page, question, name](auto&&, auto&&)
                    {
                        if (auto s = weak.get())
                        {
                            s->CopyAnswer(page, question, name);
                        }
                    });
                controls::Grid::SetColumn(copy, 2);

                grid.Children().Append(big);
                grid.Children().Append(small);
                grid.Children().Append(copy);
                entry.Child(grid);

                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(entry, winrt::hstring{ std::wstring{ name } + L", " + std::wstring{ detail } });
                m_nameResults.Children().Append(entry);
            }
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to name the chord.")
    }

    void MainWindow::UpdateKeyChords() noexcept
    {
        try
        {
            if (!m_keyChords || m_keyRoot.SelectedIndex() < 0 || m_keyScale.SelectedIndex() < 0)
            {
                return;
            }

            theory::MusicKey const key{ static_cast<uint8_t>(m_keyRoot.SelectedIndex()), static_cast<theory::ScaleKind>(m_keyScale.SelectedIndex()) };

            // the scale's notes, and the major key with the same notes when that helps
            std::wstring notes{};

            for (auto const pitchClass : theory::ScalePitchClasses(key))
            {
                notes += (notes.empty() ? L"" : L" ") + theory::NoteNameInKey(pitchClass, &key);
            }

            m_keyNotes.Inlines().Clear();
            winrt::Microsoft::UI::Xaml::Documents::Run scaleRun{};
            scaleRun.Text(notes);
            m_keyNotes.Inlines().Append(scaleRun);

            auto const parent = theory::ParentMajorRoot(key);
            auto const sevenNotes = theory::ScalePitchClasses(key).size() == 7;

            if (sevenNotes && key.Scale != theory::ScaleKind::Major && key.Scale != theory::ScaleKind::HarmonicMinor)
            {
                theory::MusicKey const major{ parent, theory::ScaleKind::Major };
                winrt::Microsoft::UI::Xaml::Documents::Run hint{};
                hint.Text(L"   " + std::wstring{ res::FormatString(L"KeySameNotesAs", theory::NoteNameInKey(parent, &major)) });
                hint.FontSize(12);
                hint.Foreground(Brush(L"TextFillColorTertiaryBrush"));
                m_keyNotes.Inlines().Append(hint);
            }

            m_keyChords.Children().Clear();
            m_keyChordList = theory::ChordsInKey(key, m_keySevenths->Selected() == 1);
            auto const& chords = m_keyChordList;

            for (size_t i = 0; i < chords.size() && i < 7; ++i)
            {
                auto const& chord = chords[i];
                // a diminished chord the way a chart of the key writes it, as in the comp: B°, which
                // the Sequencer reads as Bdim
                auto spelled = theory::ChordName(chord, &key);

                if (chord.Type == theory::ChordType::Diminished)
                {
                    spelled = theory::NoteNameInKey(chord.Root, &key) + L"\u00B0";
                }

                winrt::hstring const name{ spelled };
                winrt::hstring const numeral{ theory::RomanNumeral(chord, key) };

                controls::Button button{};
                button.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                button.HorizontalContentAlignment(xaml::HorizontalAlignment::Center);
                button.Padding(xaml::Thickness{ 4, 6, 4, 6 });
                button.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));

                controls::StackPanel words{};
                words.Spacing(2);

                controls::TextBlock big{};
                big.Text(name);
                big.FontSize(15);
                big.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
                big.HorizontalAlignment(xaml::HorizontalAlignment::Center);
                words.Children().Append(big);
                words.Children().Append(SmallText(numeral));
                button.Content(words);

                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(button, res::FormatString(L"KeyChordName", std::wstring_view{ name }, std::wstring_view{ numeral }));
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetPositionInSet(button, static_cast<int32_t>(i) + 1);
                winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetSizeOfSet(button, static_cast<int32_t>(chords.size()));

                // sends the chord to Chord to notes
                button.Click([weak = get_weak(), name](auto&&, auto&&)
                    {
                        if (auto s = weak.get())
                        {
                            s->m_chordText.Text(name);
                        }
                    });

                controls::Grid::SetColumn(button, static_cast<int32_t>(i));
                m_keyChords.Children().Append(button);
            }

            HighlightKeyChord();
        }
        MIDI_CALCULATOR_CATCH_AND_LOG(L"Unable to show the chords in the key.")
    }

    // Lights the chord in the key that Chord to notes is showing, if it's one of them.
    void MainWindow::HighlightKeyChord() noexcept
    {
        try
        {
            if (!m_keyChords)
            {
                return;
            }

            auto accent = winrt::unbox_value<winrt::Windows::UI::Color>(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"SystemAccentColorLight2")));
            accent.A = 0x24;

            for (uint32_t i = 0; i < m_keyChords.Children().Size() && i < m_keyChordList.size(); ++i)
            {
                auto const button = m_keyChords.Children().GetAt(i).as<controls::Button>();
                auto const selected = m_chord.has_value() && *m_chord == m_keyChordList[i];

                if (selected)
                {
                    button.Background(media::SolidColorBrush{ accent });
                    button.BorderBrush(Brush(L"AccentFillColorDefaultBrush"));
                }
                else
                {
                    button.ClearValue(controls::Control::BackgroundProperty());
                    button.ClearValue(controls::Control::BorderBrushProperty());
                }
            }
        }
        CATCH_LOG()
    }
}
