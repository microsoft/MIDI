// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Following the Key and chords row (proposal revision 2, section 3, phase 3): Song key and Chord
// tones on generators, and Follow key on notes clips and melodic patterns. What plays is worked
// out in HarmonyFollow.cpp, which the tests cover; this is the controls.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "Harmony.h"
#include "HarmonyFollow.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr wchar_t const* RootNames[]{ L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

        constexpr wchar_t const* ScaleNameKeys[]{
            L"ScaleMajor", L"ScaleMinor", L"ScaleDorian", L"ScalePhrygian", L"ScaleLydian", L"ScaleMixolydian",
            L"ScaleHarmonicMinor", L"ScaleMajorPentatonic", L"ScaleMinorPentatonic", L"ScaleBlues", L"ScaleChromatic",
        };
    }

    _Use_decl_annotations_
    std::optional<seq::KeyChange> MainWindow::SongKeyForClip(std::wstring const& clipId) const
    {
        if (!m_doc.Harmony || m_doc.Harmony->Keys.empty())
        {
            return std::nullopt;
        }

        // Where it's first placed; the start of the song when it isn't placed.
        int64_t first{ INT64_MAX };

        seq::ForEachTrack(m_doc, [&](seq::Track const& track, size_t)
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

        return seq::SongKeyAt(m_doc, first == INT64_MAX ? 0 : first);
    }

    _Use_decl_annotations_
    void MainWindow::FillGeneratorKeyPickers(
        controls::ComboBox const& key,
        controls::ComboBox const& scale,
        seq::Clip const& clip,
        GeneratorEdit const& edit)
    {
        auto const& generator = *clip.Generator;

        key.Items().Append(winrt::box_value(res::GetString(L"GeneratorSongKey")));

        for (auto const name : RootNames)
        {
            key.Items().Append(winrt::box_value(winrt::hstring{ name }));
        }

        for (auto const name : ScaleNameKeys)
        {
            scale.Items().Append(winrt::box_value(res::GetString(name)));
        }

        key.SelectedIndex(generator.SongKey ? 0 : static_cast<int32_t>(generator.ScaleRoot % 12) + 1);
        scale.SelectedIndex(static_cast<int32_t>(generator.Scale));

        // Following the song, the scale comes from the song too. Its own key and scale stay, for
        // where the song has none.
        scale.IsEnabled(!generator.SongKey);

        auto const help = res::GetString(m_doc.Harmony && !m_doc.Harmony->Keys.empty() ? L"GeneratorSongKeyHelp" : L"GeneratorSongKeyNoKey");
        controls::ToolTipService::SetToolTip(key, winrt::box_value(help));
        automation::AutomationProperties::SetHelpText(key, help);

        key.SelectionChanged([weak = get_weak(), edit, clipId = clip.Id](foundation::IInspectable const& sender, auto&&)
        {
            auto const selected = sender.as<controls::ComboBox>().SelectedIndex();
            auto s = weak.get();

            if (selected < 0 || !s)
            {
                return;
            }

            if (selected == 0)
            {
                // Takes the song's key where the clip is first placed, so what it draws in the
                // editor is what plays there.
                auto const songKey = s->SongKeyForClip(clipId);

                edit([songKey](seq::GeneratorSettings& edited)
                {
                    edited.SongKey = true;

                    if (songKey)
                    {
                        edited.ScaleRoot = static_cast<uint8_t>(songKey->Root % 12);
                        edited.Scale = songKey->Scale;
                    }
                });
            }
            else
            {
                edit([selected](seq::GeneratorSettings& edited)
                {
                    edited.SongKey = false;
                    edited.ScaleRoot = static_cast<uint8_t>(selected - 1);
                });
            }
        });

        scale.SelectionChanged([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const selected = sender.as<controls::ComboBox>().SelectedIndex();

            if (selected >= 0)
            {
                edit([selected](seq::GeneratorSettings& edited) { edited.Scale = static_cast<seq::ScaleKind>(selected); });
            }
        });
    }

    _Use_decl_annotations_
    xaml::UIElement MainWindow::MakeChordTonesSwitch(seq::GeneratorSettings const& generator, GeneratorEdit const& edit)
    {
        controls::CheckBox box{};
        box.Content(winrt::box_value(res::GetString(L"GeneratorChordTones")));
        box.IsChecked(generator.ChordTones);

        auto const hasChords = m_doc.Harmony && !m_doc.Harmony->Chords.empty();
        auto const help = res::GetString(hasChords ? L"GeneratorChordTonesHelp" : L"GeneratorChordTonesNoChords");
        controls::ToolTipService::SetToolTip(box, winrt::box_value(help));
        automation::AutomationProperties::SetHelpText(box, help);

        box.Click([edit](foundation::IInspectable const& sender, auto&&)
        {
            auto const checked = sender.as<controls::CheckBox>().IsChecked();
            auto const on = checked != nullptr && checked.GetBoolean();
            edit([on](seq::GeneratorSettings& edited) { edited.ChordTones = on; });
        });

        return box;
    }

    _Use_decl_annotations_
    void MainWindow::UpdateFollowKeyToggle(seq::Clip const& clip) noexcept
    {
        try
        {
            auto const can = clip.Kind == seq::ClipKind::Notes ||
                (clip.Kind == seq::ClipKind::Pattern && clip.Pattern.has_value() && clip.Pattern->Mode == seq::PatternMode::Melodic);
            auto const hasKey = m_doc.Harmony && !m_doc.Harmony->Keys.empty();

            EditorFollowKeyToggle().Visibility(can ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            EditorFollowKeyToggle().IsChecked(clip.FollowKey);

            // Turning it on needs a key to say what the notes were written in. Turning it off
            // never does.
            EditorFollowKeyToggle().IsEnabled(hasKey || clip.FollowKey);

            winrt::hstring tip{};

            if (!hasKey && !clip.FollowKey)
            {
                tip = res::GetString(L"FollowKeyNoKey");
            }
            else if (clip.FollowKey)
            {
                seq::KeyChange const written{ 0, clip.WrittenRoot, clip.WrittenScale };
                tip = res::FormatString(L"FollowKeyOnFormat", std::wstring{ KeyDisplayName(written) });
            }
            else
            {
                tip = res::GetString(L"FollowKeyHelp");
            }

            controls::ToolTipService::SetToolTip(EditorFollowKeyToggle(), winrt::box_value(tip));
            automation::AutomationProperties::SetHelpText(EditorFollowKeyToggle(), tip);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show Follow key.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorFollowKeyClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto const checked = EditorFollowKeyToggle().IsChecked();
        SetClipFollowsKey(m_editorClipId, checked != nullptr && checked.GetBoolean());
    }

    _Use_decl_annotations_
    void MainWindow::UseFoundKey(int64_t tick, seq::KeyChange key) noexcept
    {
        try
        {
            auto const before = m_doc.Harmony;
            auto after = m_doc.Harmony.value_or(seq::HarmonyTrack{});
            after.Hidden = false;

            key.Tick = std::max<int64_t>(0, tick);
            std::erase_if(after.Keys, [&key](seq::KeyChange const& existing) { return existing.Tick == key.Tick; });
            after.Keys.push_back(key);
            seq::NormalizeHarmony(after);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeHarmonyChange(before, after));
            m_doc.Harmony = std::move(after);
            Commit(std::wstring{ res::GetString(L"UndoFindKey") }, std::move(changes));
            ShowMessage(res::FormatString(L"FoundKeyFormat", std::wstring{ KeyDisplayName(key) }));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to use the key found.")
    }

    _Use_decl_annotations_
    void MainWindow::SetClipFollowsKey(std::wstring const& clipId, bool on) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, clipId);

            if (clip == nullptr || clip->FollowKey == on)
            {
                return;
            }

            auto changed = *clip;
            changed.FollowKey = on;

            if (on)
            {
                // Written in the song's key where it's first placed, or C major when the song
                // has no key yet.
                auto const key = SongKeyForClip(clipId).value_or(seq::KeyChange{});
                changed.WrittenRoot = static_cast<uint8_t>(key.Root % 12);
                changed.WrittenScale = key.Scale;
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipSettingsChange(*clip, changed));
            m_undo.ApplyAndCommit(m_doc, std::wstring{ res::GetString(L"UndoFollowKey") }, std::move(changes));
            DocumentChanged();

            RefreshEditor();
            InvalidateArrange();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to turn Follow key on or off.")
    }
}
