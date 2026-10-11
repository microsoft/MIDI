// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "SequencePattern.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        // The General MIDI drum map names notes 35 to 81.
        constexpr uint8_t FirstDrumName = 35;
        constexpr uint8_t LastDrumName = 81;

        // The drums a new lane gets, in the order a kit usually grows.
        constexpr std::array<uint8_t, 18> DrumOrder{ 36, 38, 42, 46, 39, 37, 49, 51, 45, 47, 50, 56, 54, 70, 44, 41, 43, 75 };

        // The ratchets offered as buttons. A file can hold 5 or 7 as well.
        constexpr std::array<uint8_t, 6> RatchetChoices{ 1, 2, 3, 4, 6, 8 };

        // Lengths offered for a lane of its own.
        constexpr std::array<uint16_t, 12> LaneLengthChoices{ 3, 4, 5, 6, 7, 8, 9, 12, 16, 24, 32, 64 };

        // About this many bars at most, so Make notes can't fill memory with a pattern that
        // takes thousands of bars to line up.
        constexpr int64_t MakeNotesMaximumBars = 64;

        struct ConditionChoice
        {
            seq::StepCondition Condition{ seq::StepCondition::Always };
            uint8_t Every{ 2 };
            uint8_t Of{ 1 };

            bool Matches(seq::PatternStep const& step) const noexcept
            {
                return step.Condition == Condition && (Condition != seq::StepCondition::Every || (step.Every == Every && step.Of == Of));
            }
        };

        std::vector<ConditionChoice> ConditionChoices(seq::PatternStep const& step)
        {
            std::vector<ConditionChoice> choices{
                { seq::StepCondition::Always, 2, 1 },
                { seq::StepCondition::FirstPass, 2, 1 },
                { seq::StepCondition::NotFirstPass, 2, 1 },
            };

            for (uint8_t every = 2; every <= 4; ++every)
            {
                for (uint8_t of = 1; of <= every; ++of)
                {
                    choices.push_back({ seq::StepCondition::Every, every, of });
                }
            }

            // One from a file that isn't in the list is shown as it is.
            if (std::none_of(choices.begin(), choices.end(), [&step](ConditionChoice const& choice) { return choice.Matches(step); }))
            {
                choices.push_back({ step.Condition, step.Every, step.Of });
            }

            return choices;
        }

        std::wstring ConditionText(ConditionChoice const& choice)
        {
            switch (choice.Condition)
            {
            case seq::StepCondition::FirstPass: return std::wstring{ res::GetString(L"ConditionFirstPass") };
            case seq::StepCondition::NotFirstPass: return std::wstring{ res::GetString(L"ConditionNotFirstPass") };
            case seq::StepCondition::Every: return std::wstring{ res::FormatString(L"ConditionEveryFormat", static_cast<uint32_t>(choice.Of), static_cast<uint32_t>(choice.Every)) };
            default: return std::wstring{ res::GetString(L"ConditionAlways") };
            }
        }

        bool SameSteps(std::vector<seq::PatternLane> const& a, std::vector<seq::PatternLane> const& b) noexcept
        {
            if (a.size() != b.size())
            {
                return false;
            }

            for (size_t i = 0; i < a.size(); ++i)
            {
                if (a[i].Id != b[i].Id || a[i].Name != b[i].Name || a[i].Note != b[i].Note || a[i].StepCount != b[i].StepCount ||
                    a[i].Muted != b[i].Muted || a[i].Soloed != b[i].Soloed || !(a[i].Steps == b[i].Steps))
                {
                    return false;
                }
            }

            return true;
        }

        bool SamePattern(seq::PatternSettings const& a, seq::PatternSettings const& b) noexcept
        {
            return a.Mode == b.Mode && a.StepCount == b.StepCount && a.StepTicks == b.StepTicks && a.Swing == b.Swing &&
                a.LanesKeepLength == b.LanesKeepLength && a.Channel == b.Channel && SameSteps(a.Lanes, b.Lanes);
        }

        controls::TextBlock SmallText(winrt::hstring const& text, media::Brush const& brush)
        {
            controls::TextBlock block{};
            block.Text(text);
            block.FontSize(12);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(brush);
            return block;
        }
    }

    bool MainWindow::EditorShowsPattern() const noexcept
    {
        if (m_editorClipId.empty())
        {
            return false;
        }

        auto const clip = seq::FindClip(m_doc, m_editorClipId);
        return clip != nullptr && clip->Kind == seq::ClipKind::Pattern && clip->Pattern.has_value();
    }

    void MainWindow::InitializePatternEditor() noexcept
    {
        try
        {
            m_pattern.SetPalette(&m_palette);

            seq::PatternStrings strings{};
            strings.DrumLanes = res::GetString(L"PatternDrumLanes");
            strings.MelodicSteps = res::GetString(L"PatternMelodicSteps");
            strings.AddLane = res::GetString(L"PatternAddLane");
            strings.NoteRow = res::GetString(L"PatternNoteRow");
            strings.NoteRowHint = res::GetString(L"PatternNoteRowHint");
            strings.AccentRow = res::GetString(L"PatternAccentRow");
            strings.SlideRow = res::GetString(L"PatternSlideRow");
            strings.MuteLetter = res::GetString(L"PatternMuteLetter");
            strings.SoloLetter = res::GetString(L"PatternSoloLetter");
            strings.BadgeFirst = res::GetString(L"PatternBadgeFirst");
            strings.BadgeNotFirst = res::GetString(L"PatternBadgeNotFirst");
            strings.UndoSteps = res::GetString(L"UndoEditSteps");
            strings.UndoVelocity = res::GetString(L"UndoStepVelocity");

            // The editor belongs to this window and only calls these while it draws.
            strings.LaneName = [this](seq::PatternLane const& lane) { return PatternLaneName(lane, seq::PatternMode::Drums); };

            strings.LaneDetail = [](seq::PatternLane const& lane, uint16_t steps)
            {
                return std::wstring{ res::FormatString(L"PatternLaneDetailFormat", seq::NoteLabel(lane.Note), static_cast<uint32_t>(steps)) };
            };

            strings.Playing = [](uint64_t pass, uint64_t step)
            {
                return std::wstring{ res::FormatString(L"PatternPlayingFormat", pass, step) };
            };

            m_pattern.SetStrings(std::move(strings));

            m_updatingPatternUi = true;

            for (auto const ticks : seq::PatternStepSizes)
            {
                PatternStepSizeCombo().Items().Append(winrt::box_value(res::GetString(L"PatternStepSize" + std::to_wstring(ticks))));
            }

            m_updatingPatternUi = false;
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to set up the pattern editor.")
    }

    _Use_decl_annotations_
    std::wstring MainWindow::PatternLaneName(seq::PatternLane const& lane, seq::PatternMode mode) const
    {
        if (!lane.Name.empty())
        {
            return lane.Name;
        }

        if (mode == seq::PatternMode::Drums && lane.Note >= FirstDrumName && lane.Note <= LastDrumName)
        {
            return std::wstring{ res::GetString(L"DrumName" + std::to_wstring(lane.Note)) };
        }

        return seq::NoteLabel(lane.Note);
    }

    void MainWindow::UpdatePatternToolbar() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Pattern.has_value())
            {
                return;
            }

            auto const& pattern = *clip->Pattern;

            m_updatingPatternUi = true;

            PatternStepsBox().Value(pattern.StepCount);
            PatternSwingBox().Value(pattern.Swing);

            int32_t sizeIndex{ -1 };

            for (size_t i = 0; i < std::size(seq::PatternStepSizes); ++i)
            {
                if (seq::PatternStepSizes[i] == pattern.StepTicks)
                {
                    sizeIndex = static_cast<int32_t>(i);
                }
            }

            PatternStepSizeCombo().SelectedIndex(sizeIndex);
            PatternKeepLengthToggle().IsChecked(pattern.LanesKeepLength);
            PatternKeepLengthToggle().Visibility(pattern.Mode == seq::PatternMode::Melodic ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            m_updatingPatternUi = false;
        }
        catch (...)
        {
            m_updatingPatternUi = false;
        }
    }

    void MainWindow::UpdatePatternFooter() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Pattern.has_value())
            {
                m_pattern.SetFooter({}, {}, {});
                return;
            }

            auto const& pattern = *clip->Pattern;

            // Where it plays.
            std::wstring left{};

            if (auto const track = EditorTrack(); track != nullptr)
            {
                left = std::wstring{ res::FormatString(L"PatternPlaysToFormat", DescribeDestination(*track)) };

                if (track->Destination.Channel < 0)
                {
                    left += L" \u00B7 " + std::wstring{ res::FormatString(L"ChannelShortFormat", static_cast<int32_t>(pattern.Channel) + 1) };
                }
            }

            // When lanes of different lengths come back together.
            std::wstring middle{};
            std::vector<uint16_t> lengths{};

            for (auto const& lane : pattern.Lanes)
            {
                auto const length = seq::LaneStepCount(pattern, lane);

                if (!lane.Steps.empty() && std::find(lengths.begin(), lengths.end(), length) == lengths.end())
                {
                    lengths.push_back(length);
                }
            }

            if (lengths.size() > 1)
            {
                std::sort(lengths.begin(), lengths.end(), std::greater<>{});

                std::wstring list{};

                for (auto const length : lengths)
                {
                    list += list.empty() ? std::to_wstring(length) : L", " + std::to_wstring(length);
                }

                auto const cycle = seq::PatternCycleSteps(pattern, false, 1000000);
                auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
                auto const bars = static_cast<double>(cycle) * static_cast<double>(pattern.StepTicks) / static_cast<double>(seq::TicksPerBar(meter));

                middle = std::wstring{ res::FormatString(L"PatternLineUpFormat", list, static_cast<uint64_t>(cycle), std::format(L"{:g}", std::round(bars * 10.0) / 10.0)) };
            }

            // The seed matters only when a step has a chance or a condition.
            std::wstring right{};

            auto const random = std::any_of(pattern.Lanes.begin(), pattern.Lanes.end(), [](seq::PatternLane const& lane)
            {
                return std::any_of(lane.Steps.begin(), lane.Steps.end(), [](seq::PatternStep const& step)
                {
                    return step.Chance < 100 || step.Condition != seq::StepCondition::Always;
                });
            });

            if (random)
            {
                right = std::wstring{ res::FormatString(L"PatternSeedFormat", std::format(L"{:04X}", clip->Seed)) };
            }

            m_pattern.SetFooter(std::move(left), std::move(middle), std::move(right));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to describe the pattern.")
    }

    // ---------------------------------------------------------------- the step inspector

    void MainWindow::UpdatePatternInspector() noexcept
    {
        try
        {
            auto const body = NoteInspectorBody();
            body.Children().Clear();

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Pattern.has_value())
            {
                return;
            }

            auto const& pattern = *clip->Pattern;
            auto const secondary = BrushFor(m_palette.Text3);

            if (!m_pattern.HasSelection())
            {
                NoteInspectorTitle().Text(res::GetString(L"PatternNoStep"));
                body.Children().Append(SmallText(res::GetString(L"PatternNoStepHint"), secondary));
                return;
            }

            auto const laneIndex = m_pattern.SelectedLane();
            auto const index = m_pattern.SelectedStep();
            auto const& lane = pattern.Lanes[laneIndex];
            auto const step = seq::FindStep(lane, index);
            auto const melodic = pattern.Mode == seq::PatternMode::Melodic;

            // "Step 7, Closed hi-hat", or the note of a melodic step.
            std::wstring const what = melodic ? (step != nullptr ? seq::NoteLabel(step->Note) : std::wstring{}) : PatternLaneName(lane, pattern.Mode);
            auto const number = static_cast<uint32_t>(index) + 1;
            NoteInspectorTitle().Text(what.empty() ? res::FormatString(L"PatternStepOnlyFormat", number) : res::FormatString(L"PatternStepTitleFormat", number, what));

            auto const note = melodic ? m_pattern.NextNote() : lane.Note;

            // An edit rebuilds this panel, so it runs after the control's event rather than inside it.
            auto const later = [queue = DispatcherQueue(), weak = get_weak()](std::function<void(MainWindow&)> action)
            {
                queue.TryEnqueue([weak, action]()
                {
                    if (auto strong = weak.get())
                    {
                        action(*strong);
                    }
                });
            };

            auto const stepEdit = [later](std::wstring name, std::function<void(seq::PatternStep&)> change)
            {
                later([name, change](MainWindow& window) { window.EditSelectedStep(name, change); });
            };

            auto const section = [&body](xaml::UIElement const& element, double top = 10)
            {
                if (auto const framework = element.try_as<xaml::FrameworkElement>())
                {
                    framework.Margin(xaml::Thickness{ 0, top, 0, 0 });
                }

                body.Children().Append(element);
            };

            // On or off.
            controls::ToggleSwitch plays{};
            plays.Header(winrt::box_value(res::GetString(L"PatternStepPlays")));
            plays.IsOn(step != nullptr);
            plays.Toggled([later, laneIndex, index, note](foundation::IInspectable const& sender, auto&&)
            {
                auto const on = sender.as<controls::ToggleSwitch>().IsOn();

                later([laneIndex, index, note, on](MainWindow& window)
                {
                    window.EditPattern(std::wstring{ res::GetString(L"UndoEditSteps") }, [=](seq::PatternSettings& edited)
                    {
                        if (laneIndex < edited.Lanes.size())
                        {
                            seq::SetStepOn(edited.Lanes[laneIndex], index, on, note);
                        }
                    });
                });
            });
            section(plays, 0);

            if (step == nullptr)
            {
                return;
            }

            // A number box that changes the selected step when it's committed.
            auto const numberBox = [&](winrt::hstring const& header, double minimum, double maximum, double value, double smallChange, std::function<void(seq::PatternStep&, double)> apply)
            {
                controls::NumberBox box{};
                box.Header(winrt::box_value(header));
                box.Minimum(minimum);
                box.Maximum(maximum);
                box.SmallChange(smallChange);
                box.LargeChange(smallChange * 10);
                box.Value(value);
                box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                box.ValueChanged([stepEdit, header, apply](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    auto const newValue = args.NewValue();

                    if (!std::isfinite(newValue) || newValue == args.OldValue())
                    {
                        return;
                    }

                    stepEdit(std::wstring{ header }, [apply, newValue](seq::PatternStep& edited) { apply(edited, newValue); });
                });

                section(box);
                return box;
            };

            // Velocity, the way values are shown elsewhere.
            auto const display = seq::AppSettings::Current().ValuesAs();

            switch (display)
            {
            case seq::ValueDisplay::Midi1:
                numberBox(res::GetString(L"PatternVelocity7"), 1, 127, seq::ScaleDown(step->Velocity, 16, 7), 1, [](seq::PatternStep& edited, double value)
                {
                    edited.Velocity = static_cast<uint16_t>(seq::ScaleUp(static_cast<uint32_t>(std::clamp(std::lround(value), 1L, 127L)), 7, 16));
                });
                break;

            case seq::ValueDisplay::Percent:
                numberBox(res::GetString(L"PatternVelocityPercent"), 0.1, 100, std::round(step->Velocity * 1000.0 / 65535.0) / 10.0, 1, [](seq::PatternStep& edited, double value)
                {
                    edited.Velocity = static_cast<uint16_t>(std::clamp(std::lround(value * 65535.0 / 100.0), 1L, 65535L));
                });
                break;

            default:
                numberBox(res::GetString(L"PatternVelocity16"), 1, 65535, step->Velocity, 256, [](seq::PatternStep& edited, double value)
                {
                    edited.Velocity = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, 65535L));
                });
                break;
            }

            // Chance.
            numberBox(res::GetString(L"PatternChance"), 0, 100, step->Chance, 5, [](seq::PatternStep& edited, double value)
            {
                edited.Chance = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 100L));
            });

            if (step->Chance == 100 || step->Chance == 0)
            {
                section(SmallText(res::GetString(step->Chance == 100 ? L"PatternChanceAlways" : L"PatternChanceNever"), secondary), 2);
            }

            // Ratchet: a row of counts, then how the repeats change.
            section(SmallText(res::GetString(L"PatternRatchet"), BrushFor(m_palette.Text2)));

            controls::StackPanel ratchets{};
            ratchets.Orientation(controls::Orientation::Horizontal);
            ratchets.Spacing(4);

            for (auto const count : RatchetChoices)
            {
                primitives::ToggleButton button{};
                button.Content(winrt::box_value(winrt::hstring{ std::to_wstring(count) }));
                button.IsChecked(step->Ratchet == count);
                button.MinWidth(28);
                button.Padding(xaml::Thickness{ 6, 3, 6, 3 });
                automation::AutomationProperties::SetName(button, res::FormatString(L"PatternRatchetCountFormat", static_cast<uint32_t>(count)));
                button.Click([stepEdit, count](auto&&, auto&&)
                {
                    stepEdit(std::wstring{ res::GetString(L"PatternRatchet") }, [count](seq::PatternStep& edited) { edited.Ratchet = count; });
                });

                ratchets.Children().Append(button);
            }

            section(ratchets, 4);

            controls::ComboBox shape{};
            automation::AutomationProperties::SetName(shape, res::GetString(L"PatternShapeName"));
            shape.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            shape.Items().Append(winrt::box_value(res::GetString(L"PatternShapeEven")));
            shape.Items().Append(winrt::box_value(res::GetString(L"PatternShapeSofter")));
            shape.Items().Append(winrt::box_value(res::GetString(L"PatternShapeLouder")));
            shape.SelectedIndex(static_cast<int32_t>(step->Shape));
            shape.IsEnabled(step->Ratchet > 1);
            shape.SelectionChanged([stepEdit](foundation::IInspectable const& sender, auto&&)
            {
                auto const selected = sender.as<controls::ComboBox>().SelectedIndex();

                if (selected >= 0)
                {
                    stepEdit(std::wstring{ res::GetString(L"PatternShapeName") }, [selected](seq::PatternStep& edited)
                    {
                        edited.Shape = static_cast<seq::RatchetShape>(selected);
                    });
                }
            });
            section(shape, 6);

            // When it plays.
            auto const choices = ConditionChoices(*step);

            controls::ComboBox condition{};
            condition.Header(winrt::box_value(res::GetString(L"PatternCondition")));
            condition.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (size_t i = 0; i < choices.size(); ++i)
            {
                condition.Items().Append(winrt::box_value(winrt::hstring{ ConditionText(choices[i]) }));

                if (choices[i].Matches(*step))
                {
                    condition.SelectedIndex(static_cast<int32_t>(i));
                }
            }

            condition.SelectionChanged([stepEdit, choices](foundation::IInspectable const& sender, auto&&)
            {
                auto const selected = sender.as<controls::ComboBox>().SelectedIndex();

                if (selected < 0 || static_cast<size_t>(selected) >= choices.size())
                {
                    return;
                }

                auto const choice = choices[static_cast<size_t>(selected)];
                stepEdit(std::wstring{ res::GetString(L"PatternCondition") }, [choice](seq::PatternStep& edited)
                {
                    edited.Condition = choice.Condition;
                    edited.Every = choice.Every;
                    edited.Of = choice.Of;
                });
            });
            section(condition);

            // Nudge and length.
            auto const reach = static_cast<double>(pattern.StepTicks / 2);

            numberBox(res::GetString(L"PatternNudge"), -reach, reach, step->Nudge, 10, [](seq::PatternStep& edited, double value)
            {
                edited.Nudge = static_cast<int16_t>(std::clamp(std::lround(value), -32768L, 32767L));
            });

            numberBox(res::GetString(L"PatternLength"), 1, seq::MaximumStepLength, step->Length, 10, [](seq::PatternStep& edited, double value)
            {
                edited.Length = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, static_cast<long>(seq::MaximumStepLength)));
            });

            // A melodic step's note, accent and slide. The grid has them too, but not for a keyboard.
            if (melodic)
            {
                auto box = numberBox(res::GetString(L"PatternNote"), 0, 127, step->Note, 1, [](seq::PatternStep& edited, double value)
                {
                    edited.Note = static_cast<uint8_t>(std::clamp(std::lround(value), 0L, 127L));
                });

                box.Description(winrt::box_value(winrt::hstring{ seq::NoteLabel(step->Note) }));

                for (auto const accent : { true, false })
                {
                    controls::CheckBox check{};
                    check.Content(winrt::box_value(res::GetString(accent ? L"PatternAccent" : L"PatternSlide")));
                    check.IsChecked(accent ? step->Accent : step->Slide);
                    check.Click([stepEdit, accent](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const on = sender.as<controls::CheckBox>().IsChecked().GetBoolean();

                        stepEdit(std::wstring{ res::GetString(L"UndoEditSteps") }, [accent, on](seq::PatternStep& edited)
                        {
                            (accent ? edited.Accent : edited.Slide) = on;
                        });
                    });

                    section(check, accent ? 10 : 0);
                }
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the selected step.")
    }

    // ---------------------------------------------------------------- edits

    _Use_decl_annotations_
    void MainWindow::ApplyPatternEdit(std::optional<seq::PatternEdit> edit) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (!edit.has_value() || clip == nullptr || !clip->Pattern.has_value())
            {
                InvalidateEditor();
                return;
            }

            auto after = std::move(edit->After);
            seq::NormalizePattern(after);

            if (SamePattern(*clip->Pattern, after))
            {
                InvalidateEditor();
                return;
            }

            if (m_readOnly)
            {
                ShowMessage(res::GetString(L"ReadOnlyEdit"));
            }

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPatternChange(m_editorClipId, *clip->Pattern, std::move(after)));

            m_undo.ApplyAndCommit(m_doc, edit->Name.empty() ? std::wstring{ res::GetString(L"UndoEditSteps") } : edit->Name, std::move(changes));
            DocumentChanged();
            RefreshEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the pattern.")
    }

    _Use_decl_annotations_
    void MainWindow::EditPattern(std::wstring const& name, std::function<void(seq::PatternSettings&)> const& edit) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || clip->Kind != seq::ClipKind::Pattern || !clip->Pattern.has_value())
            {
                return;
            }

            seq::PatternEdit change{};
            change.Name = name;
            change.After = *clip->Pattern;
            edit(change.After);

            ApplyPatternEdit(std::move(change));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the pattern.")
    }

    _Use_decl_annotations_
    void MainWindow::EditSelectedStep(std::wstring const& name, std::function<void(seq::PatternStep&)> const& edit) noexcept
    {
        if (!m_pattern.HasSelection())
        {
            return;
        }

        auto const lane = m_pattern.SelectedLane();
        auto const index = m_pattern.SelectedStep();

        EditPattern(name, [&](seq::PatternSettings& pattern)
        {
            if (lane < pattern.Lanes.size())
            {
                if (auto const step = seq::FindStep(pattern.Lanes[lane], index); step != nullptr)
                {
                    edit(*step);

                    if (pattern.Mode == seq::PatternMode::Melodic)
                    {
                        m_pattern.SetNextNote(step->Note);
                    }
                }
            }
        });
    }

    void MainWindow::AddPatternLane() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Pattern.has_value() || clip->Pattern->Lanes.size() >= seq::MaximumPatternLanes)
            {
                return;
            }

            auto const& lanes = clip->Pattern->Lanes;
            uint8_t note{ 60 };

            // The next drum a kit usually adds, or the note above the highest lane.
            auto const unused = std::find_if(DrumOrder.begin(), DrumOrder.end(), [&lanes](uint8_t candidate)
            {
                return std::none_of(lanes.begin(), lanes.end(), [candidate](seq::PatternLane const& lane) { return lane.Note == candidate; });
            });

            if (unused != DrumOrder.end())
            {
                note = *unused;
            }
            else if (!lanes.empty())
            {
                auto const highest = std::max_element(lanes.begin(), lanes.end(), [](seq::PatternLane const& a, seq::PatternLane const& b) { return a.Note < b.Note; });
                note = static_cast<uint8_t>(std::min(127, highest->Note + 1));
            }

            EditPattern(std::wstring{ res::GetString(L"UndoAddPatternLane") }, [note](seq::PatternSettings& pattern)
            {
                pattern.Lanes.push_back(seq::MakePatternLane(note, pattern.StepCount));
            });
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to add a lane.")
    }

    _Use_decl_annotations_
    void MainWindow::ShowPatternLaneMenu(size_t laneIndex, xaml::UIElement const& anchor, foundation::Point point)
    {
        auto const clip = seq::FindClip(m_doc, m_editorClipId);

        if (clip == nullptr || !clip->Pattern.has_value() || laneIndex >= clip->Pattern->Lanes.size())
        {
            return;
        }

        auto const& pattern = *clip->Pattern;
        auto const& lane = pattern.Lanes[laneIndex];
        auto const laneId = lane.Id;
        auto const count = pattern.Lanes.size();

        controls::MenuFlyout menu{};
        auto weak = get_weak();

        // Lanes are found again by id: the menu can outlive an undo that moves them.
        auto const edit = [weak, laneId](wchar_t const* undoKey, std::function<void(seq::PatternSettings&, size_t)> change)
        {
            if (auto strong = weak.get())
            {
                strong->EditPattern(std::wstring{ res::GetString(undoKey) }, [&](seq::PatternSettings& edited)
                {
                    auto const found = std::find_if(edited.Lanes.begin(), edited.Lanes.end(), [&laneId](seq::PatternLane const& candidate) { return candidate.Id == laneId; });

                    if (found != edited.Lanes.end())
                    {
                        change(edited, static_cast<size_t>(std::distance(edited.Lanes.begin(), found)));
                    }
                });
            }
        };

        auto const add = [&menu](winrt::hstring const& text, wchar_t const* glyph, std::function<void()> action, bool enabled = true)
        {
            controls::MenuFlyoutItem item{};
            item.Text(text);
            item.IsEnabled(enabled);

            if (glyph != nullptr)
            {
                controls::FontIcon icon{};
                icon.Glyph(glyph);
                item.Icon(icon);
            }

            item.Click([action](auto&&, auto&&) { action(); });
            menu.Items().Append(item);
        };

        auto const currentName = PatternLaneName(lane, pattern.Mode);

        add(res::GetString(L"MenuRenamePatternLane"), L"\uE8AC", [weak, edit, currentName]()
        {
            if (auto strong = weak.get())
            {
                strong->ShowTextPrompt(res::GetString(L"PromptPatternLaneName"), winrt::hstring{ currentName }, [edit](std::wstring const& text)
                {
                    edit(L"UndoPatternLane", [text](seq::PatternSettings& edited, size_t index) { edited.Lanes[index].Name = text.substr(0, 200); });
                });
            }
        });

        auto const currentNote = lane.Note;

        add(res::GetString(L"MenuPatternLaneNote"), L"\uE8D6", [weak, edit, currentNote]()
        {
            if (auto strong = weak.get())
            {
                strong->ShowTextPrompt(res::GetString(L"PromptPatternLaneNote"), winrt::hstring{ std::to_wstring(currentNote) }, [weak, edit](std::wstring const& text)
                {
                    try
                    {
                        size_t used{ 0 };
                        auto const value = std::stoi(text, &used);

                        if (used > 0 && value >= 0 && value <= 127)
                        {
                            edit(L"UndoPatternLane", [value](seq::PatternSettings& edited, size_t index) { edited.Lanes[index].Note = static_cast<uint8_t>(value); });
                            return;
                        }
                    }
                    catch (...)
                    {
                    }

                    if (auto again = weak.get())
                    {
                        again->ShowMessage(res::GetString(L"PatternNoteInvalid"));
                    }
                });
            }
        });

        if (pattern.Mode == seq::PatternMode::Drums)
        {
            // A length of its own. Picking one turns on lanes keeping their own length.
            controls::MenuFlyoutSubItem steps{};
            steps.Text(res::GetString(L"MenuPatternLaneSteps"));

            auto const laneSteps = seq::LaneStepCount(pattern, lane);

            for (auto const length : LaneLengthChoices)
            {
                controls::RadioMenuFlyoutItem item{};
                item.Text(res::FormatString(L"MenuPatternLaneStepsFormat", static_cast<uint32_t>(length)));
                item.GroupName(L"PatternLaneSteps");
                item.IsChecked(laneSteps == length);
                item.Click([edit, length](auto&&, auto&&)
                {
                    edit(L"UndoPatternLane", [length](seq::PatternSettings& edited, size_t index)
                    {
                        edited.Lanes[index].StepCount = length;
                        edited.LanesKeepLength = true;
                    });
                });

                steps.Items().Append(item);
            }

            menu.Items().Append(steps);
            menu.Items().Append(controls::MenuFlyoutSeparator{});

            add(res::GetString(L"MenuMovePatternLaneUp"), L"\uE70E", [edit]()
            {
                edit(L"UndoPatternLane", [](seq::PatternSettings& edited, size_t index) { if (index > 0) { std::swap(edited.Lanes[index], edited.Lanes[index - 1]); } });
            }, laneIndex > 0);

            add(res::GetString(L"MenuMovePatternLaneDown"), L"\uE70D", [edit]()
            {
                edit(L"UndoPatternLane", [](seq::PatternSettings& edited, size_t index) { if (index + 1 < edited.Lanes.size()) { std::swap(edited.Lanes[index], edited.Lanes[index + 1]); } });
            }, laneIndex + 1 < count);

            menu.Items().Append(controls::MenuFlyoutSeparator{});

            add(res::GetString(L"MenuDeletePatternLane"), L"\uE74D", [weak, edit]()
            {
                if (auto strong = weak.get())
                {
                    strong->m_pattern.ClearSelection();
                }

                edit(L"UndoDeletePatternLane", [](seq::PatternSettings& edited, size_t index)
                {
                    edited.Lanes.erase(edited.Lanes.begin() + static_cast<ptrdiff_t>(index));
                });
            }, count > 1);
        }

        primitives::FlyoutShowOptions options{};
        options.Position(point);
        menu.ShowAt(anchor, options);
    }

    void MainWindow::VaryPattern() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                return;
            }

            auto varied = *clip;

            do
            {
                varied.Seed = seq::MakeSeed();
            } while (varied.Seed == clip->Seed);

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipSettingsChange(*clip, varied));

            m_undo.ApplyAndCommit(m_doc, std::wstring{ res::GetString(L"UndoVary") }, std::move(changes));
            DocumentChanged();
            RefreshEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to vary the pattern.")
    }

    void MainWindow::MakeNotesFromPattern() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || (!clip->Pattern.has_value() && !clip->Generator.has_value()))
            {
                return;
            }

            // What an arpeggiator plays depends on where it plays, so it has its own.
            if (seq::IsArpeggiator(*clip))
            {
                MakeNotesFromArpeggiator();
                return;
            }

            auto const trackId = m_editorTrackId;
            auto const patternId = clip->Id;
            auto const patternLength = clip->Length;

            // A generator that changes each pass gives its first four passes, like comp 4.
            auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
            auto const ceiling = seq::TicksPerBar(meter) * MakeNotesMaximumBars;
            auto notes = clip->Generator.has_value() ? seq::MakeNotesFromGenerator(*clip, 4, ceiling) : seq::MakeNotesFromPattern(*clip, ceiling);
            auto const notesId = notes.Id;

            auto const before = m_doc.Tracks;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeClipPresenceChange(notes, true));
            m_doc.Clips.push_back(std::move(notes));

            // This track plays the notes clip wherever it played the pattern. A placement keeps
            // its length, so the timeline doesn't change.
            if (auto track = seq::FindTrack(m_doc, trackId); track != nullptr)
            {
                for (auto& placement : track->Timeline)
                {
                    if (placement.ClipId == patternId)
                    {
                        placement.ClipId = notesId;

                        if (placement.Length <= 0)
                        {
                            placement.Length = patternLength;
                        }
                    }
                }

                for (auto& slot : track->Slots)
                {
                    if (slot == patternId)
                    {
                        slot = notesId;
                    }
                }
            }

            changes.push_back(seq::MakeTracksChange(before, m_doc.Tracks));

            // The pattern stays while another track uses it. Undo brings it back either way.
            for (auto& unused : seq::RemoveUnusedClips(m_doc))
            {
                changes.push_back(seq::MakeClipPresenceChange(std::move(unused), false));
            }

            Commit(std::wstring{ res::GetString(L"UndoMakeNotes") }, std::move(changes));
            OpenClipInEditor(notesId, trackId);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to make notes from the pattern.")
    }

    // ---------------------------------------------------------------- pointer and keys

    _Use_decl_annotations_
    void MainWindow::OnPatternPressed(input::PointerRoutedEventArgs const& args)
    {
        auto const point = args.GetCurrentPoint(m_editorCanvas);
        auto const position = point.Position();
        auto const right = point.Properties().IsRightButtonPressed();

        m_editorCanvas.Focus(xaml::FocusState::Pointer);

        if (m_selectedLanePoint != SIZE_MAX || m_selectedTempoPoint != SIZE_MAX)
        {
            ClearAutomationSelection();
            InvalidateArrange();
        }

        auto const hit = m_pattern.HitTest(position);

        switch (hit.Target)
        {
        case seq::PatternTarget::LaneMute:
        case seq::PatternTarget::LaneSolo:
        {
            auto const mute = hit.Target == seq::PatternTarget::LaneMute;
            auto const lane = hit.Lane;

            EditPattern(std::wstring{ res::GetString(L"UndoPatternLane") }, [mute, lane](seq::PatternSettings& pattern)
            {
                if (lane < pattern.Lanes.size())
                {
                    auto& flag = mute ? pattern.Lanes[lane].Muted : pattern.Lanes[lane].Soloed;
                    flag = !flag;
                }
            });

            args.Handled(true);
            return;
        }

        case seq::PatternTarget::LaneName:
            ShowPatternLaneMenu(hit.Lane, m_editorCanvas, position);
            args.Handled(true);
            return;

        case seq::PatternTarget::AddLane:
            if (!right)
            {
                AddPatternLane();
            }

            args.Handled(true);
            return;

        default:
            break;
        }

        if (m_pattern.PointerPressed(position, right))
        {
            UpdatePatternInspector();
            InvalidateEditor();

            if (!right)
            {
                m_patternGesture = true;
                m_editorCanvas.CapturePointer(args.Pointer());
            }
        }

        args.Handled(true);
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternMoved(input::PointerRoutedEventArgs const& args)
    {
        if (m_patternGesture && m_pattern.PointerMoved(args.GetCurrentPoint(m_editorCanvas).Position()))
        {
            InvalidateEditor();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternReleased(input::PointerRoutedEventArgs const& args)
    {
        if (!m_patternGesture)
        {
            return;
        }

        m_patternGesture = false;

        auto edit = m_pattern.PointerReleased(args.GetCurrentPoint(m_editorCanvas).Position());
        m_editorCanvas.ReleasePointerCaptures();

        if (edit.has_value())
        {
            ApplyPatternEdit(std::move(edit));
        }
        else
        {
            UpdatePatternInspector();
            InvalidateEditor();
        }
    }

    _Use_decl_annotations_
    bool MainWindow::OnPatternKeyDown(winrt::Windows::System::VirtualKey key, bool shift) noexcept
    {
        try
        {
            using winrt::Windows::System::VirtualKey;

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr || !clip->Pattern.has_value() || clip->Pattern->Lanes.empty())
            {
                return false;
            }

            auto const melodic = clip->Pattern->Mode == seq::PatternMode::Melodic;
            auto const moved = [this]()
            {
                UpdatePatternInspector();
                InvalidateEditor();
                return true;
            };

            // The first arrow picks a step to start from.
            if (!m_pattern.HasSelection() && (key == VirtualKey::Left || key == VirtualKey::Right || key == VirtualKey::Up || key == VirtualKey::Down))
            {
                m_pattern.Select(0, 0);
                return moved();
            }

            if (!m_pattern.HasSelection())
            {
                return false;
            }

            auto const lane = m_pattern.SelectedLane();
            auto const index = m_pattern.SelectedStep();
            auto const step = seq::FindStep(clip->Pattern->Lanes[lane], index);

            switch (key)
            {
            case VirtualKey::Left:
                m_pattern.MoveSelection(-1, 0);
                return moved();

            case VirtualKey::Right:
                m_pattern.MoveSelection(1, 0);
                return moved();

            case VirtualKey::Up:
            case VirtualKey::Down:
            {
                auto const direction = key == VirtualKey::Up ? 1 : -1;

                // A melodic step's note goes up or down: a semitone, or an octave with Shift.
                if (melodic)
                {
                    if (step != nullptr)
                    {
                        auto const amount = direction * (shift ? 12 : 1);
                        EditSelectedStep(std::wstring{ res::GetString(L"UndoEditSteps") }, [amount](seq::PatternStep& edited)
                        {
                            edited.Note = static_cast<uint8_t>(std::clamp(static_cast<int32_t>(edited.Note) + amount, 0, 127));
                        });
                    }

                    return true;
                }

                m_pattern.MoveSelection(0, -direction);
                return moved();
            }

            case VirtualKey::Space:
            case VirtualKey::Enter:
            {
                auto const note = melodic ? m_pattern.NextNote() : clip->Pattern->Lanes[lane].Note;
                auto const on = step == nullptr;

                EditPattern(std::wstring{ res::GetString(L"UndoEditSteps") }, [lane, index, on, note](seq::PatternSettings& pattern)
                {
                    if (lane < pattern.Lanes.size())
                    {
                        seq::SetStepOn(pattern.Lanes[lane], index, on, note);
                    }
                });

                return true;
            }

            case VirtualKey::Delete:
            case VirtualKey::Back:
                if (step != nullptr)
                {
                    EditPattern(std::wstring{ res::GetString(L"UndoEditSteps") }, [lane, index](seq::PatternSettings& pattern)
                    {
                        if (lane < pattern.Lanes.size())
                        {
                            seq::SetStepOn(pattern.Lanes[lane], index, false, 0);
                        }
                    });
                }

                return true;

            case VirtualKey::Escape:
                m_pattern.ClearSelection();
                return moved();

            default:
                return false;
            }
        }
        catch (...)
        {
            return false;
        }
    }

    // ---------------------------------------------------------------- the header

    _Use_decl_annotations_
    void MainWindow::OnPatternStepsChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
    {
        if (m_updatingPatternUi)
        {
            return;
        }

        auto const value = args.NewValue();

        if (!std::isfinite(value))
        {
            UpdatePatternToolbar();
            return;
        }

        auto const count = static_cast<uint16_t>(std::clamp(std::lround(value), 1L, static_cast<long>(seq::MaximumPatternSteps)));
        EditPattern(std::wstring{ res::GetString(L"UndoPatternSettings") }, [count](seq::PatternSettings& pattern) { seq::SetPatternStepCount(pattern, count); });
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternStepSizeChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        if (m_updatingPatternUi)
        {
            return;
        }

        auto const index = PatternStepSizeCombo().SelectedIndex();

        if (index < 0 || static_cast<size_t>(index) >= std::size(seq::PatternStepSizes))
        {
            return;
        }

        auto const ticks = seq::PatternStepSizes[static_cast<size_t>(index)];
        EditPattern(std::wstring{ res::GetString(L"UndoPatternSettings") }, [ticks](seq::PatternSettings& pattern) { pattern.StepTicks = ticks; });
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternSwingChanged(controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const& args)
    {
        if (m_updatingPatternUi)
        {
            return;
        }

        auto const value = args.NewValue();

        if (!std::isfinite(value))
        {
            UpdatePatternToolbar();
            return;
        }

        auto const swing = static_cast<uint8_t>(std::clamp(std::lround(value), static_cast<long>(seq::MinimumSwing), static_cast<long>(seq::MaximumSwing)));
        EditPattern(std::wstring{ res::GetString(L"UndoPatternSettings") }, [swing](seq::PatternSettings& pattern) { pattern.Swing = swing; });
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternKeepLengthClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto const keep = PatternKeepLengthToggle().IsChecked().GetBoolean();
        EditPattern(std::wstring{ res::GetString(L"UndoPatternSettings") }, [keep](seq::PatternSettings& pattern) { pattern.LanesKeepLength = keep; });
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternVaryClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        VaryPattern();
    }

    _Use_decl_annotations_
    void MainWindow::OnPatternMakeNotesClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        MakeNotesFromPattern();
    }
}
