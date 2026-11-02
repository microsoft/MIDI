// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Ask an AI assistant on the Key and chords row (proposal revision 2, section 7, phase 5). The
// app writes a starting prompt for the person to paste into the assistant they use, and reads
// back the song plan the assistant answers with: the key, the chords and the sections, nothing
// else. The app never sends anything itself. Reading and checking the answer is HarmonySong.cpp,
// which the tests cover; the format is documented for assistants in
// docs/kb/midi-sequencer-key-and-chords-for-agents.md.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "Harmony.h"
#include "HarmonySong.h"
#include "StringResources.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // The guide the prompt sends the assistant to. Pete will have an aka.ms link for it on
        // November 2 2026; until then, its published address.
        constexpr wchar_t SongGuideAddress[] = L"https://microsoft.github.io/MIDI/kb/midi-sequencer-key-and-chords-for-agents/";

        std::wstring Number(int64_t value)
        {
            return std::to_wstring(value);
        }
    }

    _Use_decl_annotations_
    winrt::hstring MainWindow::DescribeSongProblem(seq::SongReadResult const& result)
    {
        auto const value = [&result](size_t i) { return i < result.Values.size() ? Number(result.Values[i]) : std::wstring{}; };

        switch (result.Problem)
        {
        case seq::SongProblem::Empty: return res::GetString(L"SongProblemEmpty");
        case seq::SongProblem::TooLong: return res::GetString(L"SongProblemTooLong");
        case seq::SongProblem::NoJson: return res::GetString(L"SongProblemNoJson");
        case seq::SongProblem::BadJson: return res::GetString(L"SongProblemBadJson");
        case seq::SongProblem::WrongFormat: return res::GetString(L"SongProblemWrongFormat");
        case seq::SongProblem::NewerVersion: return res::GetString(L"SongProblemNewerVersion");
        case seq::SongProblem::BadVersion: return res::GetString(L"SongProblemBadVersion");
        case seq::SongProblem::BadMeter: return res::FormatString(L"SongProblemBadMeterFormat", result.Text);
        case seq::SongProblem::MeterDiffers:
            return res::FormatString(L"SongProblemMeterDiffersFormat", value(0) + L"/" + value(1), value(2) + L"/" + value(3));
        case seq::SongProblem::BadKey:
            return result.Section.empty() ? res::GetString(L"SongProblemBadKey") : res::FormatString(L"SongProblemBadSectionKeyFormat", result.Section);
        case seq::SongProblem::NoSections: return res::GetString(L"SongProblemNoSections");
        case seq::SongProblem::TooManySections: return res::FormatString(L"SongProblemTooManySectionsFormat", seq::MaximumSongSections);
        case seq::SongProblem::BadSectionName: return res::FormatString(L"SongProblemBadSectionNameFormat", value(0), seq::MaximumSectionNameLength);
        case seq::SongProblem::BadBars: return res::FormatString(L"SongProblemBadBarsFormat", result.Section, seq::MaximumSongBars);
        case seq::SongProblem::TooManyBars: return res::FormatString(L"SongProblemTooManyBarsFormat", value(0), seq::MaximumSongBars);
        case seq::SongProblem::NoChords: return res::FormatString(L"SongProblemNoChordsFormat", result.Section);
        case seq::SongProblem::TooManyChords: return res::FormatString(L"SongProblemTooManyChordsFormat", seq::MaximumSongChords);
        case seq::SongProblem::BadChord: return res::FormatString(L"SongProblemBadChordFormat", result.Section, result.Text);
        case seq::SongProblem::BadBeats: return res::FormatString(L"SongProblemBadBeatsFormat", result.Section, result.Text);
        case seq::SongProblem::BeatsDontAddUp:
        {
            auto const bars = result.Values.size() > 1 ? result.Values[1] : 0;
            auto const perBar = result.Values.size() > 2 ? result.Values[2] : 0;
            return res::FormatString(L"SongProblemBeatsFormat", result.Section, value(0), value(1), result.Text, Number(bars * perBar));
        }
        case seq::SongProblem::None:
        default:
            return {};
        }
    }

    winrt::hstring MainWindow::BuildSongPrompt()
    {
        std::wstring prompt{ res::FormatString(L"SongPromptIntroFormat", std::wstring{ SongGuideAddress }) };
        prompt += L"\r\n\r\n";

        // What the assistant can't see but needs: the meter, the tempo, the key and the sections
        // there are, so it doesn't ask.
        auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
        auto const bpm = m_doc.Tempo.empty() ? 120.0 : m_doc.Tempo.front().BeatsPerMinute;
        prompt += res::FormatString(L"SongPromptMeterFormat",
            std::to_wstring(meter.Numerator) + L"/" + std::to_wstring(meter.Denominator), static_cast<int64_t>(std::lround(bpm)));

        // Its meter changes, so the assistant keeps each part in one meter.
        for (size_t i = 1; i < m_doc.Meter.size() && i <= seq::MaximumSongSections; ++i)
        {
            auto const& change = m_doc.Meter[i];
            prompt += L" " + std::wstring{ res::FormatString(L"SongPromptMeterChangeFormat", seq::BarPositionAtTick(m_doc.Meter, change.Tick).Bar,
                std::to_wstring(change.Numerator) + L"/" + std::to_wstring(change.Denominator)) };
        }

        prompt += L"\r\n";

        if (auto const key = m_doc.Harmony ? seq::KeyAt(*m_doc.Harmony, 0) : nullptr; key != nullptr)
        {
            prompt += res::FormatString(L"SongPromptKeyFormat", std::wstring{ KeyDisplayName(*key) },
                seq::NoteName(key->Root, seq::KeyUsesFlats(key->Root, key->Scale)), std::wstring{ seq::ScaleId(key->Scale) });
        }
        else
        {
            prompt += res::GetString(L"SongPromptNoKey");
        }

        prompt += L"\r\n";

        if (m_doc.Tags.empty())
        {
            prompt += res::GetString(L"SongPromptNoSections");
        }
        else
        {
            prompt += res::GetString(L"SongPromptSections");

            // Each tag's text, cleaned so a name can't add lines of its own.
            size_t listed{ 0 };

            for (auto const& tag : m_doc.Tags)
            {
                if (++listed > seq::MaximumSongSections)
                {
                    break;
                }

                std::wstring text{};

                for (auto const ch : tag.Text)
                {
                    text += (ch < 0x20 || ch == 0x7F) ? L' ' : ch;
                }

                prompt += L"\r\n" + std::wstring{ res::FormatString(L"SongPromptSectionFormat", seq::BarPositionAtTick(m_doc.Meter, tag.Tick).Bar, text.substr(0, 80)) };
            }
        }

        prompt += L"\r\n\r\n";
        prompt += res::GetString(L"SongPromptRequest");
        return winrt::hstring{ prompt };
    }

    void MainWindow::ShowSongAssistant() noexcept
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto const prompt = strong->BuildSongPrompt();

                controls::StackPanel panel{};
                panel.Spacing(8);
                panel.MinWidth(520);

                controls::TextBlock message{};
                message.Text(res::GetString(L"SongAssistantMessage"));
                message.TextWrapping(xaml::TextWrapping::Wrap);
                panel.Children().Append(message);

                // ---- 1. the prompt
                controls::TextBox promptBox{};
                promptBox.Header(winrt::box_value(res::GetString(L"SongAssistantPromptHeader")));
                promptBox.Text(prompt);
                promptBox.IsReadOnly(true);
                promptBox.TextWrapping(xaml::TextWrapping::Wrap);
                promptBox.AcceptsReturn(true);
                promptBox.Height(150);
                controls::ScrollViewer::SetVerticalScrollBarVisibility(promptBox, controls::ScrollBarVisibility::Auto);
                panel.Children().Append(promptBox);

                controls::StackPanel promptActions{};
                promptActions.Orientation(controls::Orientation::Horizontal);
                promptActions.Spacing(12);

                controls::Button copy{};
                copy.Content(winrt::box_value(res::GetString(L"SongAssistantCopy")));

                // Weak: the button holds this handler.
                copy.Click([weakCopy = winrt::make_weak(copy), prompt](auto&&, auto&&)
                {
                    auto button = weakCopy.get();

                    if (!button)
                    {
                        return;
                    }

                    try
                    {
                        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package{};
                        package.SetText(prompt);
                        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
                        button.Content(winrt::box_value(res::GetString(L"SongAssistantCopied")));
                    }
                    catch (...)
                    {
                        button.Content(winrt::box_value(res::GetString(L"SongAssistantCopyFailed")));
                    }
                });
                promptActions.Children().Append(copy);

                controls::HyperlinkButton guide{};
                guide.Content(winrt::box_value(res::GetString(L"SongAssistantGuideLink")));
                guide.NavigateUri(foundation::Uri{ SongGuideAddress });
                promptActions.Children().Append(guide);
                panel.Children().Append(promptActions);

                // ---- 2. the answer
                controls::TextBox answerBox{};
                answerBox.Header(winrt::box_value(res::GetString(L"SongAssistantAnswerHeader")));
                answerBox.PlaceholderText(res::GetString(L"SongAssistantAnswerPlaceholder"));
                answerBox.TextWrapping(xaml::TextWrapping::Wrap);
                answerBox.AcceptsReturn(true);
                answerBox.Height(150);
                answerBox.Margin(xaml::Thickness{ 0, 8, 0, 0 });
                controls::ScrollViewer::SetVerticalScrollBarVisibility(answerBox, controls::ScrollBarVisibility::Auto);
                panel.Children().Append(answerBox);

                // Where it goes: the whole song in place of what's there, or from the playhead.
                controls::RadioButtons where{};
                where.Header(winrt::box_value(res::GetString(L"SongAssistantWhereHeader")));
                where.Items().Append(winrt::box_value(res::GetString(L"SongAssistantReplace")));
                where.Items().Append(winrt::box_value(res::GetString(L"SongAssistantAddAtPlayhead")));
                where.SelectedIndex(strong->m_doc.Harmony && !strong->m_doc.Harmony->Chords.empty() ? 1 : 0);
                where.MaxColumns(2);
                panel.Children().Append(where);

                controls::StackPanel checkRow{};
                checkRow.Orientation(controls::Orientation::Horizontal);
                checkRow.Spacing(12);

                controls::Button check{};
                check.Content(winrt::box_value(res::GetString(L"SongAssistantCheck")));
                checkRow.Children().Append(check);

                controls::TextBlock verdict{};
                verdict.TextWrapping(xaml::TextWrapping::Wrap);
                verdict.VerticalAlignment(xaml::VerticalAlignment::Center);
                verdict.MaxWidth(400);
                checkRow.Children().Append(verdict);
                panel.Children().Append(checkRow);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Title(winrt::box_value(res::GetString(L"SongAssistantTitle")));

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                dialog.Content(scroller);
                dialog.PrimaryButtonText(res::GetString(L"SongAssistantApply"));
                dialog.CloseButtonText(res::GetString(L"SongAssistantClose"));
                dialog.IsPrimaryButtonEnabled(false);
                dialog.DefaultButton(controls::ContentDialogButton::Close);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                // The checked plan, and the answer and place it was checked for: Apply waits for a
                // check of what's there now. The handlers hold the dialog and its parts weakly,
                // since the dialog holds them.
                auto checkedPlan = std::make_shared<std::optional<seq::SongPlan>>();
                auto checkedStart = std::make_shared<int64_t>(0);
                auto const weakDialog = winrt::make_weak(dialog);
                auto const weakWhere = winrt::make_weak(where);
                auto const weakAnswer = winrt::make_weak(answerBox);
                auto const weakVerdict = winrt::make_weak(verdict);

                auto const startTick = [weak = strong->get_weak(), weakWhere]() -> int64_t
                {
                    auto s = weak.get();
                    auto w = weakWhere.get();

                    if (!s || !w || w.SelectedIndex() != 1)
                    {
                        return 0;
                    }

                    return seq::TickAtBar(s->m_doc.Meter, seq::BarPositionAtTick(s->m_doc.Meter, std::max<int64_t>(0, s->m_position)).Bar);
                };

                auto const runCheck = [weak = strong->get_weak(), weakAnswer, weakVerdict, weakDialog, checkedPlan, checkedStart, startTick]()
                {
                    auto s = weak.get();
                    auto a = weakAnswer.get();
                    auto v = weakVerdict.get();
                    auto d = weakDialog.get();

                    if (!s || !a || !v || !d)
                    {
                        return;
                    }

                    auto const start = startTick();
                    auto const meter = seq::MeterAtTick(s->m_doc.Meter, start);
                    auto const result = seq::ReadSongAnswer(std::wstring{ a.Text() }, meter);

                    if (!result.Succeeded())
                    {
                        checkedPlan->reset();
                        v.Text(s->DescribeSongProblem(result));
                        d.IsPrimaryButtonEnabled(false);
                        return;
                    }

                    auto const summary = seq::SummarizeSong(*result.Plan);

                    // Its beats were checked against one meter, so it can't run over a change.
                    if (seq::SongCrossesMeterChange(s->m_doc.Meter, start, summary.Bars))
                    {
                        checkedPlan->reset();
                        v.Text(res::GetString(L"SongProblemMeterChangesInside"));
                        d.IsPrimaryButtonEnabled(false);
                        return;
                    }

                    v.Text(res::FormatString(summary.KeyChanges == 1 ? L"SongAssistantSummaryOneKeyChangeFormat" : L"SongAssistantSummaryFormat",
                        summary.Sections, summary.Bars, summary.Chords, summary.KeyChanges));
                    *checkedPlan = result.Plan;
                    *checkedStart = start;
                    d.IsPrimaryButtonEnabled(true);
                };

                check.Click([runCheck](auto&&, auto&&) { runCheck(); });

                // A change after a check needs another one.
                auto const uncheck = [checkedPlan, weakDialog, weakVerdict]()
                {
                    checkedPlan->reset();

                    if (auto d = weakDialog.get())
                    {
                        d.IsPrimaryButtonEnabled(false);
                    }

                    if (auto v = weakVerdict.get())
                    {
                        v.Text(L"");
                    }
                };

                answerBox.TextChanged([uncheck](auto&&, auto&&) { uncheck(); });
                where.SelectionChanged([uncheck](auto&&, auto&&) { uncheck(); });

                auto const result = co_await dialog.ShowAsync();

                if (result == controls::ContentDialogResult::Primary && checkedPlan->has_value())
                {
                    strong->ApplySongPlan(**checkedPlan, *checkedStart, where.SelectedIndex() == 0);
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show Ask an AI assistant.")
        }(get_strong());
    }

    xaml::UIElement MainWindow::MakeAssistantSetting()
    {
        controls::ToggleSwitch toggle{};
        toggle.Header(winrt::box_value(res::GetString(L"AssistantSettingHeader")));
        toggle.OnContent(winrt::box_value(res::GetString(L"AutoSaveOn")));
        toggle.OffContent(winrt::box_value(res::GetString(L"AutoSaveOff")));
        toggle.IsOn(seq::AppSettings::Current().ShowAssistant());
        toggle.Margin(xaml::Thickness{ 0, 4, 0, 0 });
        controls::ToolTipService::SetToolTip(toggle, winrt::box_value(res::GetString(L"AssistantSettingHelp")));
        automation::AutomationProperties::SetHelpText(toggle, res::GetString(L"AssistantSettingHelp"));

        toggle.Toggled([](foundation::IInspectable const& sender, auto&&)
        {
            seq::AppSettings::Current().ShowAssistant(sender.as<controls::ToggleSwitch>().IsOn());
        });

        return toggle;
    }

    _Use_decl_annotations_
    void MainWindow::ApplySongPlan(seq::SongPlan const& plan, int64_t start, bool replace) noexcept
    {
        try
        {
            auto const layout = seq::LayOutSong(plan, m_doc.Meter, replace ? 0 : start);

            auto const harmonyBefore = m_doc.Harmony;
            auto after = m_doc.Harmony.value_or(seq::HarmonyTrack{});
            after.Hidden = false;

            auto tags = m_doc.Tags;

            if (replace)
            {
                after.Keys = layout.Keys;
                after.Chords = layout.Chords;
                tags = layout.Tags;
            }
            else
            {
                // The key that was playing where the song ends carries on after it.
                auto const keyAfter = seq::KeyAt(after, layout.End);
                std::optional<seq::KeyChange> resume{};

                if (keyAfter != nullptr)
                {
                    resume = *keyAfter;
                    resume->Tick = layout.End;
                }

                std::erase_if(after.Keys, [&layout](seq::KeyChange const& key) { return key.Tick >= layout.Start && key.Tick < layout.End; });
                after.Keys.insert(after.Keys.end(), layout.Keys.begin(), layout.Keys.end());

                if (resume && !std::any_of(after.Keys.begin(), after.Keys.end(), [&](seq::KeyChange const& key) { return key.Tick == layout.End; }))
                {
                    after.Keys.push_back(*resume);
                }

                seq::NormalizeHarmony(after);

                // The song's whole span is its own, so its "N.C." beats are silent.
                seq::ClearChordRange(after, layout.Start, layout.End);

                for (auto const& chord : layout.Chords)
                {
                    seq::PlaceChord(after, chord);
                }

                tags.insert(tags.end(), layout.Tags.begin(), layout.Tags.end());
                std::stable_sort(tags.begin(), tags.end(), [](seq::Tag const& a, seq::Tag const& b) { return a.Tick < b.Tick; });
            }

            seq::NormalizeHarmony(after);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeHarmonyChange(harmonyBefore, after));
            changes.push_back(seq::MakeSequenceTagsChange(m_doc.Tags, tags));
            m_doc.Harmony = std::move(after);
            m_doc.Tags = std::move(tags);

            m_selectedChord = SIZE_MAX;
            m_selectedKey = SIZE_MAX;

            Commit(std::wstring{ res::GetString(L"UndoSongFromAssistant") }, std::move(changes));

            auto const summary = seq::SummarizeSong(plan);
            ShowMessage(res::FormatString(L"SongAppliedFormat", summary.Sections, summary.Bars, summary.Chords));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to put the assistant's song on the Key and chords row.")
    }
}
