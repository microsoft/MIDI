// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The piano roll's note tools (design section 34; Pete, November 5 2026: "please plan to
// implement them in the piano roll editor ... like adding a toolbar or context menu that acts on
// the selection").
//
// - A toolbar row above the roll says how many notes are selected and has a menu for each kind of
//   tool: Select, Timing, Length, Pitch and Velocity, plus Quantize and Clean up. A right click on
//   empty space or on a selected note opens the same menus, and so do the menu key and Shift+F10.
// - Every tool changes the selected notes, or the whole clip when none are selected.
// - A tool with settings opens a panel over the note inspector. The roll shows what it will do
//   while the settings change, and nothing changes until Apply. Apply is one undo step.
// - When notes of the same number overlap on one channel, the toolbar offers to fix them: most
//   instruments stop the sound at the first note off, so the second note is cut short.
// The tools themselves are in NoteTools.cpp, with no XAML, so the unit tests use the same code.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // Counting overlaps is quick, but not for every click in a clip this big.
        constexpr size_t MostNotesToCheckForOverlaps = 50000;

        struct TickChoice
        {
            int64_t Ticks;
            wchar_t const* Key;
        };

        constexpr std::array<TickChoice, 6> GridChoices
        { {
            { 960, L"NoteToolGridQuarter" },
            { 480, L"NoteToolGridEighth" },
            { 320, L"NoteToolGridEighthTriplet" },
            { 240, L"NoteToolGridSixteenth" },
            { 160, L"NoteToolGridSixteenthTriplet" },
            { 120, L"NoteToolGridThirtySecond" },
        } };

        constexpr std::array<TickChoice, 7> LengthChoices
        { {
            { 3840, L"NoteToolLengthBar" },
            { 1920, L"NoteToolLengthHalf" },
            { 960, L"NoteToolLengthQuarter" },
            { 480, L"NoteToolLengthEighth" },
            { 240, L"NoteToolLengthSixteenth" },
            { 120, L"NoteToolLengthThirtySecond" },
            { 60, L"NoteToolLengthSixtyFourth" },
        } };

        template <size_t N>
        int32_t IndexOfTicks(std::array<TickChoice, N> const& choices, int64_t ticks) noexcept
        {
            for (size_t i = 0; i < N; ++i)
            {
                if (choices[i].Ticks == ticks)
                {
                    return static_cast<int32_t>(i);
                }
            }

            return 0;
        }

        // Where the clip first plays in the song; the start of the song when it isn't placed.
        int64_t FirstPlacement(seq::Sequence const& sequence, std::wstring const& clipId)
        {
            int64_t first{ INT64_MAX };

            seq::ForEachTrack(sequence, [&](seq::Track const& track, size_t)
            {
                for (auto const& placement : track.Timeline)
                {
                    if (placement.ClipId == clipId)
                    {
                        first = std::min(first, placement.Tick);
                    }
                }

                return true;
            });

            return first == INT64_MAX ? 0 : first;
        }

        // Puts a track just after another, wherever that one is, in a folder or not.
        bool InsertTrackAfter(std::vector<seq::Track>& tracks, std::wstring const& id, seq::Track& track)
        {
            for (size_t i = 0; i < tracks.size(); ++i)
            {
                if (tracks[i].Id == id)
                {
                    tracks.insert(tracks.begin() + static_cast<std::ptrdiff_t>(i) + 1, std::move(track));
                    return true;
                }

                if (tracks[i].IsFolder && InsertTrackAfter(tracks[i].Children, id, track))
                {
                    return true;
                }
            }

            return false;
        }

        wchar_t const* UndoNameKey(MainWindow::NoteTool tool) noexcept
        {
            using Tool = MainWindow::NoteTool;

            switch (tool)
            {
            case Tool::Quantize: return L"UndoNoteQuantize";
            case Tool::Humanize: return L"UndoNoteHumanize";
            case Tool::Groove: return L"UndoNoteGroove";
            case Tool::TimeFeel: return L"UndoNoteTimeFeel";
            case Tool::Ratchet: return L"UndoNoteRatchet";
            case Tool::Legato:
            case Tool::FixedLength:
            case Tool::PercentLength: return L"UndoNoteLength";
            case Tool::CleanUp: return L"UndoNoteCleanUp";
            case Tool::TransposeInKey: return L"UndoNoteTransposeInKey";
            case Tool::FitToChords: return L"UndoNoteFitToChords";
            case Tool::Mirror: return L"UndoNoteMirror";
            case Tool::Velocity: return L"UndoNoteVelocity";
            case Tool::ReleaseVelocity: return L"UndoNoteReleaseVelocity";
            case Tool::Chance: return L"UndoNoteChance";
            case Tool::Strum: return L"UndoNoteStrum";
            case Tool::SelectVelocity:
            default: return L"UndoEditNotes";
            }
        }

        // The panel's title and the sentence under it that says why you'd use the tool.
        std::pair<wchar_t const*, wchar_t const*> TitleKeys(MainWindow::NoteTool tool) noexcept
        {
            using Tool = MainWindow::NoteTool;

            switch (tool)
            {
            case Tool::Quantize: return { L"NoteToolQuantizeTitle", L"NoteToolQuantizeHelp" };
            case Tool::Humanize: return { L"NoteToolHumanizeTitle", L"NoteToolHumanizeHelp" };
            case Tool::Groove: return { L"NoteToolGrooveTitle", L"NoteToolGrooveHelp" };
            case Tool::TimeFeel: return { L"NoteToolTimeFeelTitle", L"NoteToolTimeFeelHelp" };
            case Tool::Ratchet: return { L"NoteToolRatchetTitle", L"NoteToolRatchetHelp" };
            case Tool::Legato: return { L"NoteToolLegatoTitle", L"NoteToolLegatoHelp" };
            case Tool::FixedLength: return { L"NoteToolFixedLengthTitle", L"NoteToolFixedLengthHelp" };
            case Tool::PercentLength: return { L"NoteToolPercentLengthTitle", L"NoteToolPercentLengthHelp" };
            case Tool::CleanUp: return { L"NoteToolCleanUpTitle", L"NoteToolCleanUpHelp" };
            case Tool::TransposeInKey: return { L"NoteToolTransposeInKeyTitle", L"NoteToolTransposeInKeyHelp" };
            case Tool::FitToChords: return { L"NoteToolFitToChordsTitle", L"NoteToolFitToChordsHelp" };
            case Tool::Mirror: return { L"NoteToolMirrorTitle", L"NoteToolMirrorHelp" };
            case Tool::Velocity: return { L"NoteToolVelocityTitle", L"NoteToolVelocityHelp" };
            case Tool::ReleaseVelocity: return { L"NoteToolReleaseVelocityTitle", L"NoteToolReleaseVelocityHelp" };
            case Tool::Chance: return { L"NoteToolChanceTitle", L"NoteToolChanceHelp" };
            case Tool::Strum: return { L"NoteToolStrumTitle", L"NoteToolStrumHelp" };
            case Tool::SelectVelocity:
            default: return { L"NoteToolSelectVelocityTitle", L"NoteToolSelectVelocityHelp" };
            }
        }
    }

    // ---------------------------------------------------------------- the toolbar

    void MainWindow::UpdateNoteToolbar() noexcept
    {
        try
        {
            auto const count = m_roll.Selection().size();
            NoteSelectionText().Text(count == 0 ? res::GetString(L"NoteSelectionNone")
                : count == 1 ? res::GetString(L"NoteSelectionOne")
                : res::FormatString(L"NoteSelectionManyFormat", count));

            size_t overlaps{ 0 };

            if (auto const clip = seq::FindClip(m_doc, m_editorClipId); clip != nullptr && clip->Kind == seq::ClipKind::Notes &&
                clip->Notes.size() <= MostNotesToCheckForOverlaps)
            {
                overlaps = seq::CountSameNoteOverlaps(clip->Notes);
            }

            NoteOverlapButton().Visibility(overlaps > 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (overlaps > 0)
            {
                NoteOverlapText().Text(overlaps == 1 ? res::GetString(L"NoteOverlapOne") : res::FormatString(L"NoteOverlapManyFormat", overlaps));
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the note tools.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNoteToolsMenuOpening(foundation::IInspectable const& sender, foundation::IInspectable const&)
    {
        try
        {
            auto const menu = sender.as<controls::MenuFlyout>();
            auto kind = NoteMenu::Select;

            if (menu == NoteTimingMenu())
            {
                kind = NoteMenu::Timing;
            }
            else if (menu == NoteLengthMenu())
            {
                kind = NoteMenu::Length;
            }
            else if (menu == NotePitchMenu())
            {
                kind = NoteMenu::Pitch;
            }
            else if (menu == NoteVelocityMenu())
            {
                kind = NoteMenu::Velocity;
            }

            // Built each time, so what's turned off matches the selection and the song now.
            menu.Items().Clear();
            FillNoteMenu(kind, menu.Items());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a note tools menu.")
    }

    _Use_decl_annotations_
    void MainWindow::OnNoteCleanUpClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ShowNoteTool(NoteTool::CleanUp);
    }

    _Use_decl_annotations_
    void MainWindow::OnNoteOverlapClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                return;
            }

            // The whole clip, whatever is selected: the button counts the whole clip.
            seq::TempoMap tempo{};
            auto const context = NoteToolContextFor(*clip, tempo);
            seq::CleanUpSettings settings{};
            settings.Short = false;

            ApplyNoteToolResult(std::wstring{ res::GetString(L"UndoNoteFixOverlaps") }, seq::CleanUp(clip->Notes, context, settings));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to fix overlapping notes.")
    }

    // ---------------------------------------------------------------- menus

    _Use_decl_annotations_
    void MainWindow::FillNoteMenu(NoteMenu menu, collections::IVector<controls::MenuFlyoutItemBase> const& items)
    {
        auto const weak = get_weak();
        auto const clip = seq::FindClip(m_doc, m_editorClipId);
        auto const hasNotes = clip != nullptr && !clip->Notes.empty();
        auto const hasSelection = !m_roll.Selection().empty();
        auto const hasKey = m_roll.Scale().has_value();

        auto const add = [&](auto&& target, wchar_t const* key, bool enabled, std::function<void(MainWindow&)> action, wchar_t const* keysKey = nullptr)
        {
            controls::MenuFlyoutItem item{};
            item.Text(res::GetString(key));
            item.IsEnabled(enabled);

            if (keysKey != nullptr)
            {
                item.KeyboardAcceleratorTextOverride(res::GetString(keysKey));
            }

            item.Click([weak, action](auto&&, auto&&)
            {
                try
                {
                    if (auto strong = weak.get())
                    {
                        action(*strong);
                    }
                }
                catch (...)
                {
                    LOG_CAUGHT_EXCEPTION();
                }
            });

            target.Append(item);
        };

        auto const separator = [&]()
        {
            items.Append(controls::MenuFlyoutSeparator{});
        };

        auto const tool = [](NoteTool which)
        {
            return [which](MainWindow& window) { window.ShowNoteTool(which); };
        };

        auto const select = [](seq::SelectKind kind, uint32_t every = 2)
        {
            return [kind, every](MainWindow& window)
            {
                seq::SelectSettings settings{};
                settings.Kind = kind;
                settings.Every = every;
                window.SelectNotesWith(settings);
            };
        };

        // A quick change that needs no settings, applied at once.
        auto const now = [](wchar_t const* undoKey, std::function<seq::NoteToolResult(MainWindow&, std::vector<seq::Note> const&, seq::NoteToolContext const&)> run)
        {
            return [undoKey, run](MainWindow& window)
            {
                auto const found = seq::FindClip(window.m_doc, window.m_editorClipId);

                if (found == nullptr)
                {
                    return;
                }

                seq::TempoMap tempo{};
                auto const context = window.NoteToolContextFor(*found, tempo);
                window.ApplyNoteToolResult(std::wstring{ res::GetString(undoKey) }, run(window, window.NoteToolTargets(), context));
            };
        };

        auto const transpose = [](int32_t semitones)
        {
            return [semitones](MainWindow& window)
            {
                if (window.m_roll.Selection().empty())
                {
                    window.m_roll.SelectAll();
                }

                window.ApplyRollEdit(window.m_roll.TransposeSelection(semitones));
            };
        };

        switch (menu)
        {
        case NoteMenu::Select:
            add(items, L"NoteMenuSelectAll", hasNotes, [](MainWindow& window)
            {
                window.m_roll.SelectAll();
                window.UpdateNoteInspector();
                window.InvalidateEditor();
            }, L"NoteMenuSelectAllKeys");
            add(items, L"NoteMenuSelectNone", hasSelection, [](MainWindow& window)
            {
                window.m_roll.ClearSelection();
                window.UpdateNoteInspector();
                window.InvalidateEditor();
            }, L"NoteMenuSelectNoneKeys");
            add(items, L"NoteMenuSelectInvert", hasNotes, select(seq::SelectKind::Invert));
            separator();
            add(items, L"NoteMenuSelectTop", hasNotes, select(seq::SelectKind::TopOfChords));
            add(items, L"NoteMenuSelectBottom", hasNotes, select(seq::SelectKind::BottomOfChords));
            add(items, L"NoteMenuSelectSame", hasSelection, select(seq::SelectKind::SameNotes));
            add(items, L"NoteMenuSelectEverySecond", hasNotes, select(seq::SelectKind::EveryNth, 2));
            add(items, L"NoteMenuSelectEveryThird", hasNotes, select(seq::SelectKind::EveryNth, 3));
            add(items, L"NoteMenuSelectEveryFourth", hasNotes, select(seq::SelectKind::EveryNth, 4));
            add(items, L"NoteMenuSelectOnBeat", hasNotes, select(seq::SelectKind::OnBeats));
            add(items, L"NoteMenuSelectOffBeat", hasNotes, select(seq::SelectKind::OffBeats));
            add(items, L"NoteMenuSelectVelocity", hasNotes, tool(NoteTool::SelectVelocity));
            add(items, L"NoteMenuSelectOutsideKey", hasNotes && hasKey, select(seq::SelectKind::OutsideKey));
            separator();
            add(items, L"NoteMenuMoveToTrack", hasSelection, [](MainWindow& window) { window.MoveSelectionToNewTrack(); });
            break;

        case NoteMenu::Timing:
            add(items, L"NoteMenuQuantizeNow", hasNotes, [](MainWindow& window) { window.OnEditorQuantizeClick(nullptr, nullptr); }, L"NoteMenuQuantizeKeys");
            add(items, L"NoteMenuQuantize", hasNotes, tool(NoteTool::Quantize));
            add(items, L"NoteMenuHumanize", hasNotes, tool(NoteTool::Humanize));
            add(items, L"NoteMenuGroove", hasNotes, tool(NoteTool::Groove));
            add(items, L"NoteMenuTimeFeel", hasNotes, tool(NoteTool::TimeFeel));
            add(items, L"NoteMenuRatchet", hasNotes, tool(NoteTool::Ratchet));
            items.Append(controls::MenuFlyoutSeparator{});
            add(items, L"NoteMenuStrum", hasNotes, tool(NoteTool::Strum));
            add(items, L"NoteMenuRemoveStrum", seq::CountStrummed(NoteToolTargets()) > 0, now(L"UndoNoteRemoveStrum", [](MainWindow&, std::vector<seq::Note> const& notes, seq::NoteToolContext const&)
            {
                return seq::RemoveStrum(notes);
            }));
            break;

        case NoteMenu::Length:
            add(items, L"NoteMenuLegato", hasNotes, tool(NoteTool::Legato));
            add(items, L"NoteMenuFixedLength", hasNotes, tool(NoteTool::FixedLength));
            add(items, L"NoteMenuPercentLength", hasNotes, tool(NoteTool::PercentLength));
            separator();
            add(items, L"NoteMenuCleanUp", hasNotes, tool(NoteTool::CleanUp));
            break;

        case NoteMenu::Pitch:
        {
            add(items, L"NoteMenuOctaveUp", hasNotes, transpose(12));
            add(items, L"NoteMenuSemitoneUp", hasNotes, transpose(1));
            add(items, L"NoteMenuSemitoneDown", hasNotes, transpose(-1));
            add(items, L"NoteMenuOctaveDown", hasNotes, transpose(-12));
            separator();
            add(items, L"NoteMenuKeyStepUp", hasNotes && hasKey, now(L"UndoNoteTransposeInKey", [](MainWindow&, std::vector<seq::Note> const& notes, seq::NoteToolContext const& context)
            {
                return seq::TransposeInKey(notes, context, 1);
            }));
            add(items, L"NoteMenuKeyStepDown", hasNotes && hasKey, now(L"UndoNoteTransposeInKey", [](MainWindow&, std::vector<seq::Note> const& notes, seq::NoteToolContext const& context)
            {
                return seq::TransposeInKey(notes, context, -1);
            }));
            add(items, L"NoteMenuTransposeInKey", hasNotes, tool(NoteTool::TransposeInKey));
            add(items, L"NoteMenuFitToChords", hasNotes && hasKey, tool(NoteTool::FitToChords));
            separator();

            controls::MenuFlyoutSubItem voicing{};
            voicing.Text(res::GetString(L"NoteMenuVoicing"));
            voicing.IsEnabled(hasNotes);

            auto const voice = [&](wchar_t const* key, seq::VoicingKind kind)
            {
                add(voicing.Items(), key, hasNotes, [kind](MainWindow& window)
                {
                    auto const found = seq::FindClip(window.m_doc, window.m_editorClipId);

                    if (found == nullptr)
                    {
                        return;
                    }

                    seq::TempoMap tempo{};
                    auto const context = window.NoteToolContextFor(*found, tempo);
                    auto result = seq::Voice(window.NoteToolTargets(), context, kind);

                    if (result.Empty())
                    {
                        window.ShowMessage(res::GetString(L"NoteToolNoChords"));
                        return;
                    }

                    window.ApplyNoteToolResult(std::wstring{ res::GetString(L"UndoNoteVoicing") }, std::move(result));
                });
            };

            voice(L"NoteMenuInversionUp", seq::VoicingKind::InversionUp);
            voice(L"NoteMenuInversionDown", seq::VoicingKind::InversionDown);
            voice(L"NoteMenuVoiceClose", seq::VoicingKind::Close);
            voice(L"NoteMenuVoiceOpen", seq::VoicingKind::Open);
            voice(L"NoteMenuDrop2", seq::VoicingKind::Drop2);
            voice(L"NoteMenuDrop3", seq::VoicingKind::Drop3);
            voice(L"NoteMenuOctaveBelow", seq::VoicingKind::OctaveBelow);
            voicing.Items().Append(controls::MenuFlyoutSeparator{});
            voice(L"NoteMenuSmoothLeading", seq::VoicingKind::SmoothLeading);
            items.Append(voicing);

            add(items, L"NoteMenuMirror", hasNotes, tool(NoteTool::Mirror));
            add(items, L"NoteMenuReverse", hasNotes, now(L"UndoNoteReverse", [](MainWindow&, std::vector<seq::Note> const& notes, seq::NoteToolContext const&)
            {
                return seq::Reverse(notes);
            }));
            break;
        }

        case NoteMenu::Velocity:
            add(items, L"NoteMenuVelocity", hasNotes, tool(NoteTool::Velocity));
            add(items, L"NoteMenuReleaseVelocity", hasNotes, tool(NoteTool::ReleaseVelocity));
            add(items, L"NoteMenuChance", hasNotes, tool(NoteTool::Chance));
            break;
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowNoteToolsContextMenu(foundation::Point position) noexcept
    {
        try
        {
            if (m_editorCanvas == nullptr || !m_roll.HasClip())
            {
                return;
            }

            controls::MenuFlyout menu{};

            for (auto const& [kind, key] : std::array<std::pair<NoteMenu, wchar_t const*>, 5>{ {
                { NoteMenu::Select, L"NoteMenuGroupSelect" },
                { NoteMenu::Timing, L"NoteMenuGroupTiming" },
                { NoteMenu::Length, L"NoteMenuGroupLength" },
                { NoteMenu::Pitch, L"NoteMenuGroupPitch" },
                { NoteMenu::Velocity, L"NoteMenuGroupVelocity" } } })
            {
                controls::MenuFlyoutSubItem group{};
                group.Text(res::GetString(key));
                FillNoteMenu(kind, group.Items());
                menu.Items().Append(group);
            }

            if (!m_roll.Selection().empty())
            {
                menu.Items().Append(controls::MenuFlyoutSeparator{});

                controls::MenuFlyoutItem remove{};
                remove.Text(res::GetString(L"NoteMenuDelete"));
                remove.KeyboardAcceleratorTextOverride(res::GetString(L"NoteMenuDeleteKeys"));
                remove.Click([weak = get_weak()](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ApplyRollEdit(strong->m_roll.DeleteSelection());
                    }
                });
                menu.Items().Append(remove);
            }

            primitives::FlyoutShowOptions options{};
            options.Position(position);
            menu.ShowAt(m_editorCanvas, options);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open the note tools menu.")
    }

    // ---------------------------------------------------------------- running a tool

    _Use_decl_annotations_
    seq::NoteToolContext MainWindow::NoteToolContextFor(seq::Clip const& clip, seq::TempoMap& tempo) const
    {
        seq::NoteToolContext context{};
        context.Meter = &m_doc.Meter;
        context.ClipStart = FirstPlacement(m_doc, clip.Id);

        if (!m_doc.Tempo.empty())
        {
            tempo = seq::TempoMap{ m_doc.Tempo };
            context.Tempo = &tempo;
        }

        // The key and chords the roll shows, for the clip it shows.
        if (clip.Id == m_editorClipId)
        {
            if (auto const& scale = m_roll.Scale(); scale.has_value())
            {
                context.InKey = scale->InKey;

                for (auto const& span : scale->Chords)
                {
                    context.Chords.push_back({ span.Start, span.End, span.Tones });
                }
            }
        }

        return context;
    }

    std::vector<seq::Note> MainWindow::NoteToolTargets() const
    {
        if (!m_roll.Selection().empty())
        {
            return m_roll.Selection();
        }

        auto const clip = seq::FindClip(m_doc, m_editorClipId);
        return clip != nullptr ? clip->Notes : std::vector<seq::Note>{};
    }

    _Use_decl_annotations_
    seq::NoteToolResult MainWindow::RunNoteTool(NoteTool tool, std::vector<seq::Note> const& notes, seq::NoteToolContext const& context) const
    {
        auto const& settings = m_noteTools;

        switch (tool)
        {
        case NoteTool::Quantize:
            return seq::Quantize(notes, context, settings.Quantize);

        case NoteTool::Humanize:
            return seq::Humanize(notes, context, settings.Humanize);

        case NoteTool::Groove:
        {
            auto const source = seq::FindClip(m_doc, settings.GrooveClipId);

            if (source == nullptr || source->Notes.empty())
            {
                return {};
            }

            seq::NoteToolContext sourceContext{};
            sourceContext.Meter = &m_doc.Meter;
            sourceContext.ClipStart = FirstPlacement(m_doc, source->Id);

            auto const groove = seq::ExtractGroove(source->Notes, sourceContext, settings.GrooveGrid);
            return seq::ApplyGroove(notes, context, groove, settings.Groove);
        }

        case NoteTool::TimeFeel:
            return seq::TimeFeel(notes, settings.TimeFeel);

        case NoteTool::Ratchet:
            return seq::Ratchet(notes, settings.Ratchet);

        case NoteTool::Legato:
            return seq::SetLengths(notes, seq::LengthSettings{ seq::LengthMode::Legato, settings.LegatoGap, 100.0 });

        case NoteTool::FixedLength:
            return seq::SetLengths(notes, seq::LengthSettings{ seq::LengthMode::Fixed, settings.FixedLength, 100.0 });

        case NoteTool::PercentLength:
            return seq::SetLengths(notes, seq::LengthSettings{ seq::LengthMode::Percent, 0, settings.LengthPercent });

        case NoteTool::CleanUp:
            return seq::CleanUp(notes, context, settings.CleanUp);

        case NoteTool::TransposeInKey:
            return seq::TransposeInKey(notes, context, settings.TransposeSteps);

        case NoteTool::FitToChords:
            return seq::FitToChords(notes, context, settings.Fit);

        case NoteTool::Mirror:
            return seq::Mirror(notes, context, static_cast<uint8_t>(std::clamp(settings.MirrorAround, 0, 127)), settings.MirrorInKey);

        case NoteTool::Velocity:
            return seq::SetValues(notes, context, settings.Velocity);

        case NoteTool::ReleaseVelocity:
            return seq::SetValues(notes, context, settings.Release);

        case NoteTool::Chance:
            return seq::SetValues(notes, context, settings.Chance);

        case NoteTool::Strum:
            return seq::Strum(notes, context, settings.Strum);

        case NoteTool::SelectVelocity:
        default:
            return {};
        }
    }

    _Use_decl_annotations_
    void MainWindow::ApplyNoteToolResult(std::wstring name, seq::NoteToolResult result) noexcept
    {
        try
        {
            if (result.Empty())
            {
                ShowMessage(res::GetString(L"NoteToolNothingToChange"));
                return;
            }

            // The selection stays what it was, with the changed notes in it as they are now;
            // nothing selected means the tool changed the whole clip, and stays that way.
            auto selection = m_roll.Selection();

            if (!selection.empty())
            {
                for (auto const& removed : result.Removed)
                {
                    auto const found = std::find(selection.begin(), selection.end(), removed);

                    if (found != selection.end())
                    {
                        selection.erase(found);
                    }
                }

                selection.insert(selection.end(), result.Added.begin(), result.Added.end());
            }

            seq::RollEdit edit{};
            edit.Name = std::move(name);
            edit.Removed = std::move(result.Removed);
            edit.Added = std::move(result.Added);
            ApplyRollEdit(std::move(edit));

            m_roll.SetSelection(std::move(selection));
            UpdateNoteInspector();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to apply a note tool.")
    }

    _Use_decl_annotations_
    void MainWindow::PreviewNoteTool(NoteTool tool, controls::TextBlock const& summary) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                return;
            }

            seq::TempoMap tempo{};
            auto const context = NoteToolContextFor(*clip, tempo);

            if (tool == NoteTool::SelectVelocity)
            {
                auto const picked = seq::SelectNotes(clip->Notes, m_roll.Selection(), context, m_noteTools.SelectVelocity);
                summary.Text(picked.size() == 1 ? res::GetString(L"NoteToolSelectsOne") : res::FormatString(L"NoteToolSelectsFormat", picked.size()));
                return;
            }

            auto result = RunNoteTool(tool, NoteToolTargets(), context);

            if (result.Empty())
            {
                summary.Text(res::GetString(L"NoteToolNoChange"));
            }
            else if (!result.Removed.empty())
            {
                summary.Text(result.Removed.size() == 1 ? res::GetString(L"NoteToolChangesOne") : res::FormatString(L"NoteToolChangesFormat", result.Removed.size()));
            }
            else
            {
                summary.Text(result.Added.size() == 1 ? res::GetString(L"NoteToolAddsOne") : res::FormatString(L"NoteToolAddsFormat", result.Added.size()));
            }

            m_roll.SetPreview(std::move(result.Removed), std::move(result.Added));
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show what a note tool will do.")
    }

    _Use_decl_annotations_
    void MainWindow::SelectNotesWith(seq::SelectSettings settings) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                return;
            }

            seq::TempoMap tempo{};
            auto const context = NoteToolContextFor(*clip, tempo);
            auto picked = seq::SelectNotes(clip->Notes, m_roll.Selection(), context, settings);

            if (picked.empty())
            {
                ShowMessage(res::GetString(L"NoteSelectNoneFound"));
            }

            m_roll.SetSelection(std::move(picked));
            UpdateNoteInspector();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to select notes.")
    }

    void MainWindow::MoveSelectionToNewTrack() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);
            auto const track = EditorTrack();
            auto const selection = m_roll.Selection();

            if (clip == nullptr || track == nullptr || selection.empty())
            {
                return;
            }

            // The new clip is a copy of this one with only the selected notes, played wherever
            // this track plays this clip, on the same destination: nothing sounds different until
            // you change it.
            seq::Clip moved = *clip;
            moved.Id = seq::NewId(L"c");
            moved.Name = std::wstring{ res::FormatString(L"MovedNotesClipNameFormat", clip->Name) };
            moved.Notes = selection;
            moved.Events.clear();
            moved.Lanes.clear();
            seq::SortClip(moved);

            auto newTrack = seq::MakeTrack(m_doc, seq::NextName(m_doc, track->Name), false);
            newTrack.Destination = track->Destination;
            newTrack.Slots.resize(m_doc.Scenes.size());

            for (auto const& placement : track->Timeline)
            {
                if (placement.ClipId == clip->Id)
                {
                    auto copy = placement;
                    copy.ClipId = moved.Id;
                    newTrack.Timeline.push_back(copy);
                }
            }

            auto const sourceTrackId = track->Id;
            auto const tracksBefore = m_doc.Tracks;

            // The pointers into the sequence aren't used after this: it's about to change.
            seq::ChangeList changes{};
            auto notesOut = seq::MakeNoteChange(m_editorClipId, selection, {});
            notesOut->Apply(m_doc);
            changes.push_back(std::move(notesOut));

            changes.push_back(seq::MakeClipPresenceChange(moved, true));
            m_doc.Clips.push_back(std::move(moved));

            if (!InsertTrackAfter(m_doc.Tracks, sourceTrackId, newTrack))
            {
                m_doc.Tracks.push_back(std::move(newTrack));
            }

            changes.push_back(seq::MakeTracksChange(tracksBefore, m_doc.Tracks));

            m_roll.ClearSelection();
            Commit(std::wstring{ res::GetString(L"UndoMoveNotesToTrack") }, std::move(changes));

            RefreshEditor();
            InvalidateArrange();
            ShowMessage(selection.size() == 1 ? res::GetString(L"MovedNoteToTrack") : res::FormatString(L"MovedNotesToTrackFormat", selection.size()));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to move the notes to a new track.")
    }

    // ---------------------------------------------------------------- a tool's panel

    _Use_decl_annotations_
    void MainWindow::ShowNoteTool(NoteTool tool) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                return;
            }

            auto const weak = get_weak();
            auto const [titleKey, helpKey] = TitleKeys(tool);

            controls::StackPanel panel{};
            panel.Spacing(10);
            panel.Width(280);

            controls::TextBlock title{};
            title.Text(res::GetString(titleKey));
            title.FontSize(14);
            title.FontWeight(winrt::Windows::UI::Text::FontWeight{ 600 });
            automation::AutomationProperties::SetHeadingLevel(title, automation::Peers::AutomationHeadingLevel::Level2);
            panel.Children().Append(title);

            auto const addText = [&](winrt::hstring const& text)
            {
                controls::TextBlock block{};
                block.Text(text);
                block.TextWrapping(xaml::TextWrapping::Wrap);
                block.FontSize(12);
                block.Foreground(BrushFor(m_palette.Text2));
                panel.Children().Append(block);
                return block;
            };

            addText(res::GetString(helpKey));

            // The settings go two to a row.
            controls::Grid fields{};
            fields.ColumnSpacing(12);
            fields.RowSpacing(10);
            fields.ColumnDefinitions().Append(controls::ColumnDefinition{});
            fields.ColumnDefinitions().Append(controls::ColumnDefinition{});
            panel.Children().Append(fields);

            int32_t cell{ 0 };

            auto const place = [&](xaml::FrameworkElement const& element, bool wide)
            {
                if (wide && cell % 2 == 1)
                {
                    ++cell;
                }

                auto const row = cell / 2;

                while (static_cast<int32_t>(fields.RowDefinitions().Size()) <= row)
                {
                    controls::RowDefinition definition{};
                    definition.Height(xaml::GridLengthHelper::Auto());
                    fields.RowDefinitions().Append(definition);
                }

                controls::Grid::SetRow(element, row);
                controls::Grid::SetColumn(element, cell % 2);

                if (wide)
                {
                    controls::Grid::SetColumnSpan(element, 2);
                    ++cell;
                }

                element.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                fields.Children().Append(element);
                ++cell;
            };

            controls::TextBlock summary{};
            summary.FontSize(12);
            summary.TextWrapping(xaml::TextWrapping::Wrap);

            auto const refresh = [weak, tool, summary]()
            {
                if (auto strong = weak.get())
                {
                    strong->PreviewNoteTool(tool, summary);
                }
            };

            auto const number = [&](wchar_t const* headerKey, double minimum, double maximum, double value, double step,
                std::function<void(NoteToolSettings&, double)> set, bool wide = false)
            {
                controls::NumberBox box{};
                box.Header(winrt::box_value(res::GetString(headerKey)));
                automation::AutomationProperties::SetName(box, res::GetString(headerKey));
                box.Minimum(minimum);
                box.Maximum(maximum);
                box.SmallChange(step);
                box.LargeChange(step * 5.0);
                box.Value(value);
                box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                box.ValueChanged([weak, set, refresh](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    if (!std::isfinite(args.NewValue()))
                    {
                        return;
                    }

                    try
                    {
                        if (auto strong = weak.get())
                        {
                            set(strong->m_noteTools, args.NewValue());
                            refresh();
                        }
                    }
                    catch (...)
                    {
                        LOG_CAUGHT_EXCEPTION();
                    }
                });
                place(box, wide);
                return box;
            };

            auto const choice = [&](wchar_t const* headerKey, std::vector<wchar_t const*> const& itemKeys, int32_t selected,
                std::function<void(NoteToolSettings&, int32_t)> set, bool wide = true)
            {
                controls::ComboBox box{};
                box.Header(winrt::box_value(res::GetString(headerKey)));
                automation::AutomationProperties::SetName(box, res::GetString(headerKey));

                for (auto const key : itemKeys)
                {
                    box.Items().Append(winrt::box_value(res::GetString(key)));
                }

                box.SelectedIndex(std::clamp<int32_t>(selected, 0, static_cast<int32_t>(itemKeys.size()) - 1));
                box.SelectionChanged([weak, set, refresh](foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const&)
                {
                    auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                    if (index < 0)
                    {
                        return;
                    }

                    try
                    {
                        if (auto strong = weak.get())
                        {
                            set(strong->m_noteTools, index);
                            refresh();
                        }
                    }
                    catch (...)
                    {
                        LOG_CAUGHT_EXCEPTION();
                    }
                });
                place(box, wide);
                return box;
            };

            auto const check = [&](wchar_t const* textKey, bool value, std::function<void(NoteToolSettings&, bool)> set)
            {
                controls::CheckBox box{};
                box.Content(winrt::box_value(res::GetString(textKey)));
                box.IsChecked(value);
                box.Click([weak, set, refresh](foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
                {
                    if (auto strong = weak.get())
                    {
                        auto const checked = sender.as<controls::CheckBox>().IsChecked();
                        set(strong->m_noteTools, checked != nullptr && checked.GetBoolean());
                        refresh();
                    }
                });
                place(box, true);
                return box;
            };

            // A new seed: the same kind of change, different details.
            auto const reroll = [&](std::function<void(NoteToolSettings&)> next)
            {
                controls::Button button{};
                button.Content(winrt::box_value(res::GetString(L"NoteToolTryAnother")));
                automation::AutomationProperties::SetHelpText(button, res::GetString(L"NoteToolTryAnotherHelp"));
                button.Click([weak, next, refresh](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        next(strong->m_noteTools);
                        refresh();
                    }
                });
                place(button, true);
            };

            std::vector<wchar_t const*> gridKeys{};

            for (auto const& grid : GridChoices)
            {
                gridKeys.push_back(grid.Key);
            }

            auto const curveKeys = std::vector<wchar_t const*>{ L"NoteToolCurveStraight", L"NoteToolCurveFastFirst", L"NoteToolCurveSlowFirst", L"NoteToolCurveS" };

            switch (tool)
            {
            case NoteTool::Quantize:
            {
                auto& quantize = m_noteTools.Quantize;

                // Opens on the snap grid the first time.
                if (SnapGrid() > 0 && quantize.Grid == seq::QuantizeSettings{}.Grid)
                {
                    quantize.Grid = SnapGrid();
                }

                choice(L"NoteToolGrid", gridKeys, IndexOfTicks(GridChoices, quantize.Grid), [](NoteToolSettings& s, int32_t i) { s.Quantize.Grid = GridChoices[static_cast<size_t>(i)].Ticks; });
                number(L"NoteToolStrength", 0, 100, quantize.Strength * 100.0, 5, [](NoteToolSettings& s, double v) { s.Quantize.Strength = v / 100.0; });
                number(L"NoteToolSwing", 50, 75, quantize.Swing * 100.0, 1, [](NoteToolSettings& s, double v) { s.Quantize.Swing = v / 100.0; });
                number(L"NoteToolKeep", 0, 50, quantize.Keep * 100.0, 5, [](NoteToolSettings& s, double v) { s.Quantize.Keep = v / 100.0; }, true);
                check(L"NoteToolQuantizeEnds", quantize.Ends, [](NoteToolSettings& s, bool v) { s.Quantize.Ends = v; });
                break;
            }

            case NoteTool::Humanize:
            {
                auto const& humanize = m_noteTools.Humanize;
                number(L"NoteToolTimingMs", 0, 60, humanize.TimingMs, 1, [](NoteToolSettings& s, double v) { s.Humanize.TimingMs = v; });
                number(L"NoteToolVelocityPercent", 0, 50, humanize.VelocityPercent, 1, [](NoteToolSettings& s, double v) { s.Humanize.VelocityPercent = v; });
                number(L"NoteToolLengthPercent", 0, 50, humanize.LengthPercent, 1, [](NoteToolSettings& s, double v) { s.Humanize.LengthPercent = v; });
                check(L"NoteToolTighterOnBeat", humanize.TighterOnBeat, [](NoteToolSettings& s, bool v) { s.Humanize.TighterOnBeat = v; });
                reroll([](NoteToolSettings& s) { ++s.Humanize.Seed; });
                break;
            }

            case NoteTool::Groove:
            {
                // Every other notes clip with notes in it, by name.
                auto ids = std::make_shared<std::vector<std::wstring>>();
                controls::ComboBox clips{};
                clips.Header(winrt::box_value(res::GetString(L"NoteToolGrooveClip")));
                automation::AutomationProperties::SetName(clips, res::GetString(L"NoteToolGrooveClip"));
                int32_t selected{ -1 };

                for (auto const& other : m_doc.Clips)
                {
                    if (other.Id == clip->Id || other.Kind != seq::ClipKind::Notes || other.Notes.empty())
                    {
                        continue;
                    }

                    if (other.Id == m_noteTools.GrooveClipId)
                    {
                        selected = static_cast<int32_t>(ids->size());
                    }

                    ids->push_back(other.Id);
                    clips.Items().Append(winrt::box_value(winrt::hstring{ other.Name }));
                }

                if (ids->empty())
                {
                    addText(res::GetString(L"NoteToolGrooveNoClips"));
                    break;
                }

                if (selected < 0)
                {
                    selected = 0;
                    m_noteTools.GrooveClipId = ids->front();
                }

                clips.SelectedIndex(selected);
                clips.SelectionChanged([weak, ids, refresh](foundation::IInspectable const& sender, controls::SelectionChangedEventArgs const&)
                {
                    auto const index = sender.as<controls::ComboBox>().SelectedIndex();

                    if (auto strong = weak.get(); strong && index >= 0 && static_cast<size_t>(index) < ids->size())
                    {
                        strong->m_noteTools.GrooveClipId = (*ids)[static_cast<size_t>(index)];
                        refresh();
                    }
                });
                place(clips, true);

                choice(L"NoteToolGrid", gridKeys, IndexOfTicks(GridChoices, m_noteTools.GrooveGrid), [](NoteToolSettings& s, int32_t i) { s.GrooveGrid = GridChoices[static_cast<size_t>(i)].Ticks; });
                number(L"NoteToolGrooveTiming", 0, 100, m_noteTools.Groove.TimingStrength * 100.0, 5, [](NoteToolSettings& s, double v) { s.Groove.TimingStrength = v / 100.0; });
                number(L"NoteToolGrooveVelocity", 0, 100, m_noteTools.Groove.VelocityStrength * 100.0, 5, [](NoteToolSettings& s, double v) { s.Groove.VelocityStrength = v / 100.0; });
                break;
            }

            case NoteTool::TimeFeel:
                choice(L"NoteToolTimeFeelDirection", { L"NoteToolSpeedUp", L"NoteToolSlowDown" }, m_noteTools.TimeFeel.SpeedUp ? 0 : 1,
                    [](NoteToolSettings& s, int32_t i) { s.TimeFeel.SpeedUp = i == 0; });
                number(L"NoteToolAmount", 0, 100, m_noteTools.TimeFeel.Amount * 100.0, 5, [](NoteToolSettings& s, double v) { s.TimeFeel.Amount = v / 100.0; });
                break;

            case NoteTool::Ratchet:
            {
                auto const& ratchet = m_noteTools.Ratchet;
                number(L"NoteToolRepeats", 2, 32, ratchet.Count, 1, [](NoteToolSettings& s, double v) { s.Ratchet.Count = static_cast<uint32_t>(std::lround(v)); });
                number(L"NoteToolGate", 5, 100, ratchet.Gate * 100.0, 5, [](NoteToolSettings& s, double v) { s.Ratchet.Gate = v / 100.0; });
                choice(L"NoteToolRatchetSpacing", { L"NoteToolSpacingEven", L"NoteToolSpeedUp", L"NoteToolSlowDown" },
                    ratchet.Spacing == seq::CurveShape::FastFirst ? 1 : ratchet.Spacing == seq::CurveShape::SlowFirst ? 2 : 0,
                    [](NoteToolSettings& s, int32_t i) { s.Ratchet.Spacing = i == 1 ? seq::CurveShape::FastFirst : i == 2 ? seq::CurveShape::SlowFirst : seq::CurveShape::Straight; });
                number(L"NoteToolAmount", 0, 100, ratchet.SpacingAmount * 100.0, 5, [](NoteToolSettings& s, double v) { s.Ratchet.SpacingAmount = v / 100.0; });
                number(L"NoteToolLastVelocity", 0, 200, ratchet.EndVelocity * 100.0, 5, [](NoteToolSettings& s, double v) { s.Ratchet.EndVelocity = v / 100.0; });
                break;
            }

            case NoteTool::Legato:
                number(L"NoteToolLegatoGap", 0, 960, static_cast<double>(m_noteTools.LegatoGap), 10, [](NoteToolSettings& s, double v) { s.LegatoGap = std::llround(v); }, true);
                break;

            case NoteTool::FixedLength:
            {
                std::vector<wchar_t const*> lengthKeys{};

                for (auto const& length : LengthChoices)
                {
                    lengthKeys.push_back(length.Key);
                }

                choice(L"NoteToolLength", lengthKeys, IndexOfTicks(LengthChoices, m_noteTools.FixedLength), [](NoteToolSettings& s, int32_t i) { s.FixedLength = LengthChoices[static_cast<size_t>(i)].Ticks; });
                break;
            }

            case NoteTool::PercentLength:
                number(L"NoteToolPercent", 1, 400, m_noteTools.LengthPercent, 5, [](NoteToolSettings& s, double v) { s.LengthPercent = v; }, true);
                break;

            case NoteTool::CleanUp:
            {
                auto const& cleanUp = m_noteTools.CleanUp;
                check(L"NoteToolCleanOverlaps", cleanUp.Overlaps, [](NoteToolSettings& s, bool v) { s.CleanUp.Overlaps = v; });
                check(L"NoteToolCleanDuplicates", cleanUp.Duplicates, [](NoteToolSettings& s, bool v) { s.CleanUp.Duplicates = v; });
                check(L"NoteToolCleanShort", cleanUp.Short, [](NoteToolSettings& s, bool v) { s.CleanUp.Short = v; });
                number(L"NoteToolShortMs", 1, 100, cleanUp.ShortMs, 1, [](NoteToolSettings& s, double v) { s.CleanUp.ShortMs = v; }, true);
                break;
            }

            case NoteTool::TransposeInKey:
                if (!m_roll.Scale().has_value())
                {
                    addText(res::GetString(L"NoteToolNoKeySemitones"));
                }

                number(L"NoteToolSteps", -21, 21, m_noteTools.TransposeSteps, 1, [](NoteToolSettings& s, double v) { s.TransposeSteps = static_cast<int32_t>(std::lround(v)); }, true);
                break;

            case NoteTool::FitToChords:
                choice(L"NoteToolFitWhich", { L"NoteToolFitAll", L"NoteToolFitOnBeats", L"NoteToolFitLong" }, static_cast<int32_t>(m_noteTools.Fit),
                    [](NoteToolSettings& s, int32_t i) { s.Fit = static_cast<seq::FitWhich>(i); });
                break;

            case NoteTool::Mirror:
            {
                controls::TextBlock name{};
                name.FontSize(12);
                name.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                name.Margin(xaml::Thickness{ 0, 0, 0, 6 });
                name.Text(winrt::hstring{ seq::NoteLabel(static_cast<uint8_t>(std::clamp(m_noteTools.MirrorAround, 0, 127))) });

                number(L"NoteToolMirrorAround", 0, 127, m_noteTools.MirrorAround, 1, [name](NoteToolSettings& s, double v)
                {
                    s.MirrorAround = std::clamp(static_cast<int32_t>(std::lround(v)), 0, 127);
                    name.Text(winrt::hstring{ seq::NoteLabel(static_cast<uint8_t>(s.MirrorAround)) });
                });
                place(name, false);

                check(L"NoteToolMirrorInKey", m_noteTools.MirrorInKey, [](NoteToolSettings& s, bool v) { s.MirrorInKey = v; });
                break;
            }

            case NoteTool::Velocity:
            case NoteTool::ReleaseVelocity:
            case NoteTool::Chance:
            {
                auto const which = [tool](NoteToolSettings& s) -> seq::ValueSettings&
                {
                    return tool == NoteTool::Velocity ? s.Velocity : tool == NoteTool::ReleaseVelocity ? s.Release : s.Chance;
                };

                auto const& values = which(m_noteTools);

                // Only the boxes the chosen way of changing uses are shown.
                auto const firstBox = std::make_shared<controls::NumberBox>(nullptr);
                auto const secondBox = std::make_shared<controls::NumberBox>(nullptr);
                auto const curveBox = std::make_shared<controls::ComboBox>(nullptr);
                auto const amountBox = std::make_shared<controls::NumberBox>(nullptr);
                auto const another = std::make_shared<controls::Button>(nullptr);

                auto const show = [firstBox, secondBox, curveBox, amountBox, another](seq::ValueMode mode)
                {
                    auto const visible = [](bool on) { return on ? xaml::Visibility::Visible : xaml::Visibility::Collapsed; };
                    auto const ramp = mode == seq::ValueMode::Ramp;

                    (*firstBox).Header(winrt::box_value(res::GetString(
                        mode == seq::ValueMode::Set ? L"NoteToolValueTo" :
                        mode == seq::ValueMode::Change ? L"NoteToolValueBy" :
                        mode == seq::ValueMode::Scale ? L"NoteToolValueScale" :
                        mode == seq::ValueMode::Ramp ? L"NoteToolValueFrom" :
                        mode == seq::ValueMode::Accent ? L"NoteToolValueAccent" : L"NoteToolValueRandom")));
                    automation::AutomationProperties::SetName(*firstBox, winrt::unbox_value_or<winrt::hstring>((*firstBox).Header(), L""));
                    (*firstBox).Minimum(mode == seq::ValueMode::Change ? -100.0 : 0.0);
                    (*firstBox).Maximum(mode == seq::ValueMode::Scale ? 300.0 : 100.0);
                    (*secondBox).Visibility(visible(ramp));
                    (*curveBox).Visibility(visible(ramp));
                    (*amountBox).Visibility(visible(ramp));
                    (*another).Visibility(visible(mode == seq::ValueMode::Random));
                };

                choice(L"NoteToolValueMode",
                    { L"NoteToolModeSet", L"NoteToolModeChange", L"NoteToolModeScale", L"NoteToolModeRamp", L"NoteToolModeAccent", L"NoteToolModeRandom" },
                    static_cast<int32_t>(values.Mode),
                    [which, show, firstBox](NoteToolSettings& s, int32_t i)
                    {
                        auto& target = which(s);
                        target.Mode = static_cast<seq::ValueMode>(i);

                        // A fresh start for each way of changing, from the middle.
                        target.First = target.Mode == seq::ValueMode::Scale ? 100.0 : target.Mode == seq::ValueMode::Set || target.Mode == seq::ValueMode::Ramp ? 75.0 : 10.0;
                        show(target.Mode);
                        (*firstBox).Value(target.First);
                    });

                *firstBox = number(L"NoteToolValueTo", -100, 300, values.First, 1, [which](NoteToolSettings& s, double v) { which(s).First = v; });
                *secondBox = number(L"NoteToolValueToRamp", 0, 100, values.Second, 1, [which](NoteToolSettings& s, double v) { which(s).Second = v; });
                *curveBox = choice(L"NoteToolCurve", curveKeys, static_cast<int32_t>(values.Curve), [which](NoteToolSettings& s, int32_t i) { which(s).Curve = static_cast<seq::CurveShape>(i); }, false);
                *amountBox = number(L"NoteToolCurveAmount", 0, 100, values.CurveAmount * 100.0, 5, [which](NoteToolSettings& s, double v) { which(s).CurveAmount = v / 100.0; });

                controls::Button button{};
                button.Content(winrt::box_value(res::GetString(L"NoteToolTryAnother")));
                automation::AutomationProperties::SetHelpText(button, res::GetString(L"NoteToolTryAnotherHelp"));
                button.Click([weak, which, refresh](auto&&, auto&&)
                {
                    if (auto strong = weak.get())
                    {
                        ++which(strong->m_noteTools).Seed;
                        refresh();
                    }
                });
                place(button, true);
                *another = button;

                show(values.Mode);

                // The mode change sets a new starting value, which the box has to show.
                (*firstBox).Value(values.First);
                break;
            }

            case NoteTool::Strum:
            {
                auto const& strum = m_noteTools.Strum;
                auto const shapeKeys = std::vector<wchar_t const*>{ L"StrumShapeEven", L"StrumShapeSpeedingUp", L"StrumShapeSlowingDown", L"StrumShapeEase", L"StrumShapeRake" };

                // The curve has a string for each note of the biggest chord it strums.
                seq::TempoMap curveTempo{};
                size_t strings{ 3 };

                for (auto const& chord : seq::FindChords(NoteToolTargets(), NoteToolContextFor(*clip, curveTempo), 30.0))
                {
                    strings = std::max(strings, chord.size());
                }

                auto curve = std::make_shared<StrumCurveState>();
                curve->Shape = strum.Shape;
                curve->Bend = strum.Bend;
                curve->Ticks = strum.InTicks;
                curve->Spread = strum.InTicks ? static_cast<double>(strum.SpreadTicks) : strum.SpreadMs;
                curve->MostSpread = strum.InTicks ? 240.0 : 150.0;
                curve->Strings = std::min<size_t>(strings, 8);

                auto const spreadBox = std::make_shared<controls::NumberBox>(nullptr);
                auto const canvas = std::make_shared<canvasXaml::CanvasControl>(nullptr);

                choice(L"StrumShape", shapeKeys, static_cast<int32_t>(strum.Shape), [curve, canvas](NoteToolSettings& s, int32_t i)
                {
                    s.Strum.Shape = static_cast<seq::StrumShape>(i);
                    curve->Shape = s.Strum.Shape;
                    if (*canvas != nullptr) { (*canvas).Invalidate(); }
                });

                // Dragging the curve shows the strum in the roll as it goes.
                auto const setSpread = [curve](NoteToolSettings& s, double value)
                {
                    if (curve->Ticks) { s.Strum.SpreadTicks = std::llround(value); } else { s.Strum.SpreadMs = value; }
                    curve->Spread = value;
                };

                *canvas = MakeStrumCurve(curve, [weak, setSpread, refresh](double value)
                {
                    if (auto strong = weak.get())
                    {
                        setSpread(strong->m_noteTools, value);
                        refresh();
                    }
                }, [spreadBox](double value)
                {
                    if (*spreadBox != nullptr) { (*spreadBox).Value(value); }
                });
                place(*canvas, true);

                *spreadBox = number(strum.InTicks ? L"StrumSpreadTicks" : L"StrumSpreadMs", 0, strum.InTicks ? 960 : 500, curve->Spread, 1, [setSpread, canvas](NoteToolSettings& s, double v)
                {
                    setSpread(s, v);
                    if (*canvas != nullptr) { (*canvas).Invalidate(); }
                });

                number(L"StrumBend", 0, 100, strum.Bend * 100.0, 5, [curve, canvas](NoteToolSettings& s, double v)
                {
                    s.Strum.Bend = v / 100.0;
                    curve->Bend = s.Strum.Bend;
                    if (*canvas != nullptr) { (*canvas).Invalidate(); }
                });

                // Changing what the spread is measured in changes the box and the curve with it.
                choice(L"StrumMeasure", { L"StrumMeasureMs", L"StrumMeasureTicks" }, strum.InTicks ? 1 : 0, [curve, canvas, spreadBox](NoteToolSettings& s, int32_t i)
                {
                    s.Strum.InTicks = i == 1;
                    curve->Ticks = s.Strum.InTicks;
                    curve->MostSpread = s.Strum.InTicks ? 240.0 : 150.0;
                    curve->Spread = s.Strum.InTicks ? static_cast<double>(s.Strum.SpreadTicks) : s.Strum.SpreadMs;

                    if (*spreadBox != nullptr)
                    {
                        auto const key = s.Strum.InTicks ? L"StrumSpreadTicks" : L"StrumSpreadMs";
                        (*spreadBox).Header(winrt::box_value(res::GetString(key)));
                        automation::AutomationProperties::SetName(*spreadBox, res::GetString(key));
                        (*spreadBox).Maximum(s.Strum.InTicks ? 960 : 500);
                        (*spreadBox).Value(curve->Spread);
                    }

                    if (*canvas != nullptr) { (*canvas).Invalidate(); }
                });
                choice(L"StrumDirection", { L"StrumDirectionDown", L"StrumDirectionUp", L"StrumDirectionByBeat", L"StrumDirectionRandom" }, static_cast<int32_t>(strum.Direction),
                    [](NoteToolSettings& s, int32_t i) { s.Strum.Direction = static_cast<seq::StrumDirection>(i); });
                choice(L"StrumLands", { L"StrumLandsFirst", L"StrumLandsMiddle", L"StrumLandsLast" }, static_cast<int32_t>(strum.Lands),
                    [](NoteToolSettings& s, int32_t i) { s.Strum.Lands = static_cast<seq::StrumLands>(i); });
                number(L"StrumTilt", -50, 50, strum.Tilt * 100.0, 5, [](NoteToolSettings& s, double v) { s.Strum.Tilt = v / 100.0; });
                number(L"StrumLoose", 0, 100, strum.Loose * 100.0, 5, [](NoteToolSettings& s, double v) { s.Strum.Loose = v / 100.0; });
                check(L"StrumKeepBass", strum.KeepBass, [](NoteToolSettings& s, bool v) { s.Strum.KeepBass = v; });
                reroll([](NoteToolSettings& s) { ++s.Strum.Seed; });
                addText(res::GetString(L"NoteToolStrumNote"));
                break;
            }

            case NoteTool::SelectVelocity:
                number(L"NoteToolLowest", 0, 100, m_noteTools.SelectVelocity.Low, 1, [](NoteToolSettings& s, double v) { s.SelectVelocity.Low = v; });
                number(L"NoteToolHighest", 0, 100, m_noteTools.SelectVelocity.High, 1, [](NoteToolSettings& s, double v) { s.SelectVelocity.High = v; });
                break;
            }

            // What it will do, and the buttons.
            summary.Foreground(BrushFor(m_palette.Text2));
            panel.Children().Append(summary);

            controls::StackPanel buttons{};
            buttons.Orientation(controls::Orientation::Horizontal);
            buttons.Spacing(8);
            buttons.HorizontalAlignment(xaml::HorizontalAlignment::Right);

            controls::Flyout flyout{};
            auto const weakFlyout = winrt::make_weak(flyout);

            controls::Button apply{};
            apply.Content(winrt::box_value(res::GetString(tool == NoteTool::SelectVelocity ? L"NoteToolSelect" : L"NoteToolApply")));
            apply.Style(xaml::Application::Current().Resources().Lookup(winrt::box_value(L"AccentButtonStyle")).as<xaml::Style>());
            apply.Click([weak, weakFlyout, tool](auto&&, auto&&)
            {
                try
                {
                    auto strong = weak.get();

                    if (strong == nullptr)
                    {
                        return;
                    }

                    strong->m_roll.ClearPreview();

                    if (tool == NoteTool::SelectVelocity)
                    {
                        strong->SelectNotesWith(strong->m_noteTools.SelectVelocity);
                    }
                    else if (auto const found = seq::FindClip(strong->m_doc, strong->m_editorClipId); found != nullptr)
                    {
                        seq::TempoMap tempo{};
                        auto const context = strong->NoteToolContextFor(*found, tempo);
                        strong->ApplyNoteToolResult(std::wstring{ res::GetString(UndoNameKey(tool)) }, strong->RunNoteTool(tool, strong->NoteToolTargets(), context));
                    }

                    if (auto open = weakFlyout.get())
                    {
                        open.Hide();
                    }
                }
                catch (...)
                {
                    LOG_CAUGHT_EXCEPTION();
                }
            });

            controls::Button cancel{};
            cancel.Content(winrt::box_value(res::GetString(L"NoteToolCancel")));
            cancel.Click([weakFlyout](auto&&, auto&&)
            {
                if (auto open = weakFlyout.get())
                {
                    open.Hide();
                }
            });

            buttons.Children().Append(apply);
            buttons.Children().Append(cancel);
            panel.Children().Append(buttons);

            flyout.Content(panel);
            flyout.Closed([weak](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->m_roll.ClearPreview();
                    strong->InvalidateEditor();
                }
            });

            // Over the note inspector, so the notes it changes stay in sight.
            auto const inspector = EditorInspector();

            if (inspector.ActualWidth() > 40)
            {
                primitives::FlyoutShowOptions options{};
                options.Position(foundation::Point{ 0, 0 });
                options.Placement(primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
                flyout.ShowAt(inspector, options);
            }
            else
            {
                flyout.Placement(primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
                flyout.ShowAt(NoteTimingButton());
            }

            refresh();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show a note tool.")
    }
}
