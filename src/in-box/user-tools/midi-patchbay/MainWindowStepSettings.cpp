// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Settings short enough to edit right in the inspector, so those steps have no dialog.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "ThemeBrushes.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace patchbay = ::midipatchbay;
namespace resources = ::midipatchbay::resources;

namespace winrt::midipatchbay::implementation
{
    namespace
    {
        controls::TextBlock Label(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(12);
            block.VerticalAlignment(xaml::VerticalAlignment::Center);
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));

            return block;
        }

        controls::TextBlock Hint(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(11);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return block;
        }

        // A line that follows a setting, such as which note middle C becomes.
        controls::TextBlock Caption() noexcept
        {
            controls::TextBlock block{};

            block.FontSize(13);
            block.TextWrapping(xaml::TextWrapping::Wrap);

            return block;
        }

        controls::NumberBox Number(
            _In_ winrt::hstring const& header,
            _In_ double minimum,
            _In_ double maximum,
            _In_ double value) noexcept
        {
            controls::NumberBox box{};

            box.Header(winrt::box_value(header));
            box.Minimum(minimum);
            box.Maximum(maximum);
            box.SmallChange(1);
            box.LargeChange(10);
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(value);

            return box;
        }

        controls::RadioButtons Choices(
            _In_ winrt::hstring const& header,
            _In_ std::initializer_list<winrt::hstring> items,
            _In_ int32_t selected) noexcept
        {
            controls::RadioButtons list{};

            list.Header(winrt::box_value(header));

            for (auto const& item : items)
            {
                list.Items().Append(winrt::box_value(item));
            }

            list.SelectedIndex(selected);

            return list;
        }

        // Sixteen choices counted from 1 on screen and stored from 0.
        controls::ComboBox GroupChoices(_In_ int32_t selected) noexcept
        {
            controls::ComboBox box{};

            box.Header(winrt::box_value(resources::GetString(L"GeneratorGroup")));
            box.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

            for (int32_t i = 0; i < 16; i++)
            {
                box.Items().Append(winrt::box_value(resources::FormatString(L"FilterGroupFormat", i + 1)));
            }

            box.SelectedIndex(std::clamp(selected, 0, 15));

            return box;
        }

        controls::CheckBox Check(_In_ winrt::hstring const& text, _In_ bool value) noexcept
        {
            controls::CheckBox check{};

            check.Content(winrt::box_value(text));
            check.IsChecked(value);

            return check;
        }

        // Two places, the same as a summary shows.
        double RoundedTempo(_In_ double value) noexcept
        {
            return std::round(std::clamp(value,
                patchbay::MinimumGeneratorBeatsPerMinute,
                patchbay::MaximumGeneratorBeatsPerMinute) * 100.0) / 100.0;
        }

        winrt::hstring SwingText(_In_ double percent) noexcept
        {
            auto const swing = static_cast<int>(std::lround(percent));

            return swing <= 50
                ? resources::GetString(L"GeneratorSwingStraight")
                : resources::FormatString(L"GeneratorSwingValueFormat", swing);
        }

        winrt::hstring TransposeExample(_In_ patchbay::BlockSettings const& settings) noexcept
        {
            // Middle C, so the example means something to a musician.
            constexpr uint8_t exampleNote = 60;

            return resources::FormatString(L"TransformTransposeExampleFormat",
                patchbay::DescribeNote(exampleNote),
                patchbay::DescribeNote(settings.Transform.ResultingNote(exampleNote)));
        }

        winrt::hstring DividerText(_In_ uint32_t division) noexcept
        {
            return resources::FormatString(L"DividerEffectFormat",
                patchbay::DescribeNumber(120.0 / (std::max)(division, 1u), 2));
        }
    }

    _Use_decl_annotations_
    bool MainWindow::EditsInInspector(patchbay::BlockKind kind) noexcept
    {
        switch (kind)
        {
        case patchbay::BlockKind::ChannelFilter:
        case patchbay::BlockKind::GroupFilter:
        case patchbay::BlockKind::VelocityFilter:
        case patchbay::BlockKind::Transpose:
        case patchbay::BlockKind::Throttle:
        case patchbay::BlockKind::ClockDivider:
        case patchbay::BlockKind::ClockGenerator:
        case patchbay::BlockKind::TimeCodeGenerator:
        case patchbay::BlockKind::NoteDistributor:
        case patchbay::BlockKind::CiResponder:
        case patchbay::BlockKind::CiFilter:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    std::optional<patchbay::BlockSettings> MainWindow::ChangeStepSettings(
        std::wstring const& blockId,
        std::function<void(patchbay::BlockSettings&)> const& change) noexcept
    {
        try
        {
            auto* patch = CurrentPatch();
            auto* target = patch == nullptr ? nullptr : patch->FindBlock(blockId);

            if (target == nullptr)
            {
                return std::nullopt;
            }

            auto settings = target->Settings;

            change(settings);

            // The same as the dialog keeps: every part a kind uses is on.
            settings.Filter.IsActive = true;
            settings.Transform.IsActive = true;

            // Through the file format and back, so what is kept is what a saved patch reads back as.
            settings = patchbay::BlockSettingsFromJson(target->Kind, patchbay::BlockSettingsToJson(target->Kind, settings));

            if (patchbay::BlockSettingsSignature(target->Kind, settings) ==
                patchbay::BlockSettingsSignature(target->Kind, target->Settings))
            {
                return std::nullopt;
            }

            target->Settings = settings;

            if (m_inspectorSummary != nullptr)
            {
                m_inspectorSummary.Text(patchbay::DescribeBlock(target->Kind, settings));
            }

            // The inspector stays as it is, so the control being used keeps its focus.
            CommitChange(true, false);
            RebuildCanvas();

            return settings;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to change the step's settings.")

        return std::nullopt;
    }

    _Use_decl_annotations_
    void MainWindow::FocusStepSettings(std::wstring const& blockId) noexcept
    {
        try
        {
            if (m_canvas.SelectionKind() != patchbay::CanvasSelectionKind::Block ||
                m_canvas.SelectedNodeId() != blockId ||
                m_canvas.SelectedNodeIds().size() != 1)
            {
                m_canvas.Select(patchbay::CanvasSelectionKind::Block, blockId);
            }

            // After the press that got here has finished, or it takes the focus back.
            DispatcherQueue().TryEnqueue([weak = get_weak()]()
                {
                    auto strong = weak.get();

                    if (strong != nullptr && strong->m_stepSettingsFocus != nullptr)
                    {
                        strong->m_stepSettingsFocus.Focus(xaml::FocusState::Programmatic);
                    }
                });
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to go to the step's settings.")
    }

    _Use_decl_annotations_
    void MainWindow::BuildStepSettings(patchbay::PatchBlock const& block, controls::StackPanel const& body) noexcept
    {
        try
        {
            auto const blockId = block.Id;
            auto const& settings = block.Settings;
            auto weak = get_weak();

            // Changes one part of the step, from any control below.
            auto const change = [weak, blockId](std::function<void(patchbay::BlockSettings&)> const& apply)
                -> std::optional<patchbay::BlockSettings>
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_updatingStepSettings)
                    {
                        return std::nullopt;
                    }

                    return strong->ChangeStepSettings(blockId, apply);
                };

            switch (block.Kind)
            {
            case patchbay::BlockKind::ChannelFilter:
            case patchbay::BlockKind::GroupFilter:
            {
                auto const isChannels = block.Kind == patchbay::BlockKind::ChannelFilter;

                controls::Grid header{};
                header.ColumnDefinitions().Append(controls::ColumnDefinition{});

                controls::ColumnDefinition linksColumn{};
                linksColumn.Width(xaml::GridLengthHelper::Auto());
                header.ColumnDefinitions().Append(linksColumn);

                header.Children().Append(Label(resources::GetString(isChannels
                    ? L"InspectorChannelsThrough"
                    : L"InspectorGroupsThrough")));

                controls::StackPanel links{};
                links.Orientation(controls::Orientation::Horizontal);
                controls::Grid::SetColumn(links, 1);

                header.Children().Append(links);
                body.Children().Append(header);

                controls::VariableSizedWrapGrid grid{};
                grid.Orientation(controls::Orientation::Horizontal);
                grid.MaximumRowsOrColumns(4);
                grid.ItemWidth(60);
                grid.ItemHeight(32);

                auto checks = std::make_shared<std::vector<controls::CheckBox>>();

                for (size_t i = 0; i < 16; i++)
                {
                    auto const value = isChannels ? settings.Filter.Channels[i] : settings.Groups[i];

                    // Numbered from one, the same as a musician counts them.
                    auto check = Check(winrt::to_hstring(i + 1), value);
                    check.MinWidth(0);

                    xaml::Automation::AutomationProperties::SetName(check, resources::FormatString(
                        isChannels ? L"FilterChannelFormat" : L"FilterGroupFormat", static_cast<int>(i) + 1));

                    auto const set = [change, isChannels, i](bool on)
                        {
                            change([isChannels, i, on](patchbay::BlockSettings& s)
                                {
                                    (isChannels ? s.Filter.Channels[i] : s.Groups[i]) = on;
                                });
                        };

                    check.Checked([set](auto&&, auto&&) { set(true); });
                    check.Unchecked([set](auto&&, auto&&) { set(false); });

                    grid.Children().Append(check);
                    checks->push_back(check);

                    if (i == 0)
                    {
                        m_stepSettingsFocus = check;
                    }
                }

                for (auto const all : { true, false })
                {
                    controls::HyperlinkButton link{};

                    link.Content(winrt::box_value(resources::GetString(all ? L"InspectorAll" : L"InspectorNone")));
                    link.Padding(xaml::ThicknessHelper::FromLengths(6, 2, 6, 2));

                    xaml::Automation::AutomationProperties::SetName(link,
                        resources::GetString(all ? L"FilterSelectAll" : L"FilterSelectNone"));

                    // One change, so one undo puts all sixteen back.
                    link.Click([weak, change, checks, isChannels, all](auto&&, auto&&)
                        {
                            auto strong = weak.get();

                            if (strong == nullptr)
                            {
                                return;
                            }

                            strong->m_updatingStepSettings = true;

                            for (auto const& check : *checks)
                            {
                                check.IsChecked(all);
                            }

                            strong->m_updatingStepSettings = false;

                            change([isChannels, all](patchbay::BlockSettings& s)
                                {
                                    for (size_t i = 0; i < 16; i++)
                                    {
                                        (isChannels ? s.Filter.Channels[i] : s.Groups[i]) = all;
                                    }
                                });
                        });

                    links.Children().Append(link);
                }

                body.Children().Append(grid);
                body.Children().Append(Hint(resources::GetString(isChannels
                    ? L"FilterSectionChannelsHint"
                    : L"FilterSectionGroupsHint")));
                break;
            }

            case patchbay::BlockKind::VelocityFilter:
            {
                auto const& range = settings.Velocities;

                auto action = Choices(resources::GetString(L"FilterActionHeader"),
                    { resources::GetString(L"FilterActionLetThrough"), resources::GetString(L"FilterActionKeepOut") },
                    range.Action == patchbay::FilterAction::KeepOut ? 1 : 0);

                action.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const list = sender.try_as<controls::RadioButtons>();

                        if (list == nullptr || list.SelectedIndex() < 0)
                        {
                            return;
                        }

                        auto const keepOut = list.SelectedIndex() == 1;

                        change([keepOut](patchbay::BlockSettings& s)
                            {
                                s.Velocities.Action = keepOut ? patchbay::FilterAction::KeepOut : patchbay::FilterAction::LetThrough;
                            });
                    });

                body.Children().Append(action);

                auto scale = Choices(resources::GetString(L"TransformValueScale"),
                    { resources::GetString(L"TransformValueScaleSevenBit"), resources::GetString(L"TransformValueScalePercent") },
                    range.Scale == patchbay::ValueScale::Percent ? 1 : 0);

                body.Children().Append(scale);

                controls::Grid boxes{};
                boxes.ColumnSpacing(8);
                boxes.ColumnDefinitions().Append(controls::ColumnDefinition{});
                boxes.ColumnDefinitions().Append(controls::ColumnDefinition{});

                auto lowBox = Number(resources::GetString(L"FilterVelocityLowest"), 0, 127, 0);
                auto highBox = Number(resources::GetString(L"FilterVelocityHighest"), 0, 127, 127);
                controls::Grid::SetColumn(highBox, 1);

                boxes.Children().Append(lowBox);
                boxes.Children().Append(highBox);
                body.Children().Append(boxes);

                // The maximum moves before the value does, or a 127 would be clamped to 100 on its
                // way through a box that is about to show percent.
                auto const applyScale = [weak, lowBox, highBox](patchbay::VelocityRange const& velocities)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        auto const isPercent = velocities.Scale == patchbay::ValueScale::Percent;

                        winrt::Windows::Globalization::NumberFormatting::DecimalFormatter formatter{};

                        formatter.IntegerDigits(1);
                        formatter.FractionDigits(isPercent ? 2 : 0);
                        formatter.IsGrouped(false);

                        strong->m_updatingStepSettings = true;

                        for (auto const& [box, hundredths] : { std::pair{ lowBox, velocities.LowestHundredths },
                                                               std::pair{ highBox, velocities.HighestHundredths } })
                        {
                            box.NumberFormatter(formatter);
                            box.Maximum(isPercent ? 100.0 : 127.0);
                            box.Value(patchbay::DisplayFromHundredths(hundredths, velocities.Scale));
                        }

                        strong->m_updatingStepSettings = false;
                    };

                applyScale(range);

                for (auto const low : { true, false })
                {
                    (low ? lowBox : highBox).ValueChanged([change, low](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                        {
                            if (std::isnan(args.NewValue()))
                            {
                                return;
                            }

                            auto const value = args.NewValue();

                            change([low, value](patchbay::BlockSettings& s)
                                {
                                    auto const hundredths = patchbay::HundredthsFromDisplay(value, s.Velocities.Scale);
                                    (low ? s.Velocities.LowestHundredths : s.Velocities.HighestHundredths) = hundredths;
                                });
                        });
                }

                scale.SelectionChanged([change, applyScale](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const list = sender.try_as<controls::RadioButtons>();

                        if (list == nullptr || list.SelectedIndex() < 0)
                        {
                            return;
                        }

                        auto const percent = list.SelectedIndex() == 1;

                        if (auto const changed = change([percent](patchbay::BlockSettings& s)
                            {
                                s.Velocities.Scale = percent ? patchbay::ValueScale::Percent : patchbay::ValueScale::SevenBit;
                            }))
                        {
                            applyScale(changed->Velocities);
                        }
                    });

                body.Children().Append(Hint(resources::GetString(L"FilterSectionVelocitiesHint")));

                m_stepSettingsFocus = action;
                break;
            }

            case patchbay::BlockKind::Transpose:
            {
                auto semitones = Number(resources::GetString(L"TransformSemitones"),
                    patchbay::MinimumTranspose, patchbay::MaximumTranspose, settings.Transform.TransposeSemitones);
                semitones.LargeChange(12);

                auto example = Caption();
                example.Text(TransposeExample(settings));

                semitones.ValueChanged([change, example](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        auto const value = static_cast<int32_t>(std::clamp(args.NewValue(),
                            static_cast<double>(patchbay::MinimumTranspose),
                            static_cast<double>(patchbay::MaximumTranspose)));

                        if (auto const changed = change([value](patchbay::BlockSettings& s) { s.Transform.TransposeSemitones = value; }))
                        {
                            example.Text(TransposeExample(*changed));
                        }
                    });

                body.Children().Append(semitones);
                body.Children().Append(example);
                body.Children().Append(Hint(resources::GetString(L"TransformSectionTransposeHint")));

                auto exactPitch = Check(resources::GetString(L"TransformIgnoreExactPitch"), settings.Transform.IgnoreExactPitchNotes);

                auto const setExactPitch = [change](bool on)
                    {
                        change([on](patchbay::BlockSettings& s) { s.Transform.IgnoreExactPitchNotes = on; });
                    };

                exactPitch.Checked([setExactPitch](auto&&, auto&&) { setExactPitch(true); });
                exactPitch.Unchecked([setExactPitch](auto&&, auto&&) { setExactPitch(false); });

                body.Children().Append(exactPitch);
                body.Children().Append(Hint(resources::GetString(L"TransformIgnoreExactPitchHint")));

                m_stepSettingsFocus = semitones;
                break;
            }

            case patchbay::BlockKind::Throttle:
            {
                // The speeds Network MIDI Setup offers, and whatever else a file asked for.
                std::vector<uint32_t> options{ 0, 1, 2, 4, 8, 16, 32 };

                if (std::find(options.begin(), options.end(), settings.SendSpeedLimit) == options.end())
                {
                    options.push_back(settings.SendSpeedLimit);
                }

                controls::ComboBox speed{};

                speed.Header(winrt::box_value(resources::GetString(L"InspectorSendingSpeed")));
                speed.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                for (size_t i = 0; i < options.size(); i++)
                {
                    speed.Items().Append(winrt::box_value(patchbay::DescribeSendSpeed(options[i])));

                    if (options[i] == settings.SendSpeedLimit)
                    {
                        speed.SelectedIndex(static_cast<int32_t>(i));
                    }
                }

                speed.SelectionChanged([change, options](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();
                        auto const index = combo == nullptr ? -1 : combo.SelectedIndex();

                        if (index < 0 || static_cast<size_t>(index) >= options.size())
                        {
                            return;
                        }

                        auto const value = options[static_cast<size_t>(index)];

                        change([value](patchbay::BlockSettings& s) { s.SendSpeedLimit = value; });
                    });

                body.Children().Append(speed);
                body.Children().Append(Hint(resources::GetString(L"InspectorSendingSpeedHelp")));

                m_stepSettingsFocus = speed;
                break;
            }

            case patchbay::BlockKind::ClockDivider:
            {
                auto divideBy = Number(resources::GetString(L"DividerDivideBy"),
                    1, patchbay::MaximumClockDivision, settings.ClockDivision);

                auto effect = Caption();
                effect.Text(DividerText(settings.ClockDivision));

                divideBy.ValueChanged([change, effect](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        auto const value = static_cast<uint32_t>(std::lround(std::clamp(args.NewValue(),
                            1.0, static_cast<double>(patchbay::MaximumClockDivision))));

                        if (auto const changed = change([value](patchbay::BlockSettings& s) { s.ClockDivision = value; }))
                        {
                            effect.Text(DividerText(changed->ClockDivision));
                        }
                    });

                body.Children().Append(divideBy);
                body.Children().Append(effect);
                body.Children().Append(Hint(resources::GetString(L"DividerHint")));

                m_stepSettingsFocus = divideBy;
                break;
            }

            case patchbay::BlockKind::ClockGenerator:
            {
                auto const& clock = settings.Clock;

                auto tempo = Number(resources::GetString(L"GeneratorBeatsPerMinute"),
                    patchbay::MinimumGeneratorBeatsPerMinute, patchbay::MaximumGeneratorBeatsPerMinute, clock.BeatsPerMinute);

                tempo.ValueChanged([change](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (!std::isnan(args.NewValue()))
                        {
                            auto const value = RoundedTempo(args.NewValue());
                            change([value](patchbay::BlockSettings& s) { s.Clock.BeatsPerMinute = value; });
                        }
                    });

                body.Children().Append(tempo);

                auto startStop = Check(resources::GetString(L"GeneratorSendStartStop"), clock.SendStartStop);

                auto const setStartStop = [change](bool on)
                    {
                        change([on](patchbay::BlockSettings& s) { s.Clock.SendStartStop = on; });
                    };

                startStop.Checked([setStartStop](auto&&, auto&&) { setStartStop(true); });
                startStop.Unchecked([setStartStop](auto&&, auto&&) { setStartStop(false); });

                body.Children().Append(startStop);
                body.Children().Append(Hint(resources::GetString(L"GeneratorSendStartStopHint")));

                auto swing = Number(resources::GetString(L"GeneratorSwingPercent"), 50, 75, clock.SwingPercent);

                auto swingText = Caption();
                swingText.FontSize(12);
                swingText.Text(SwingText(clock.SwingPercent));

                swing.ValueChanged([change, swingText](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        if (std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        auto const value = std::round(std::clamp(args.NewValue(), 50.0, 75.0));

                        if (auto const changed = change([value](patchbay::BlockSettings& s) { s.Clock.SwingPercent = value; }))
                        {
                            swingText.Text(SwingText(changed->Clock.SwingPercent));
                        }
                    });

                body.Children().Append(swing);

                controls::ComboBox subdivision{};

                subdivision.Header(winrt::box_value(resources::GetString(L"GeneratorSwingAppliesTo")));
                subdivision.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                subdivision.Items().Append(winrt::box_value(resources::GetString(L"GeneratorSwingEighths")));
                subdivision.Items().Append(winrt::box_value(resources::GetString(L"GeneratorSwingSixteenths")));
                subdivision.SelectedIndex(clock.SwingSubdivision == 4 ? 1 : 0);

                subdivision.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            auto const value = combo.SelectedIndex() == 1 ? 4 : 2;
                            change([value](patchbay::BlockSettings& s) { s.Clock.SwingSubdivision = value; });
                        }
                    });

                body.Children().Append(subdivision);
                body.Children().Append(swingText);

                auto group = GroupChoices(clock.Group);

                group.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            auto const value = static_cast<uint8_t>(combo.SelectedIndex());
                            change([value](patchbay::BlockSettings& s) { s.Clock.Group = value; });
                        }
                    });

                body.Children().Append(group);
                body.Children().Append(Hint(resources::GetString(L"GeneratorGroupHint")));

                m_stepSettingsFocus = tempo;
                break;
            }

            case patchbay::BlockKind::TimeCodeGenerator:
            {
                auto const& timeCode = settings.TimeCode;

                // In the order of the format's own rate codes, so the index is the code.
                controls::ComboBox rate{};

                rate.Header(winrt::box_value(resources::GetString(L"GeneratorFrameRate")));
                rate.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                for (auto const each : { midiapp::MidiTimeCodeFrameRate::Frames24, midiapp::MidiTimeCodeFrameRate::Frames25,
                                         midiapp::MidiTimeCodeFrameRate::Frames2997Drop, midiapp::MidiTimeCodeFrameRate::Frames30 })
                {
                    rate.Items().Append(winrt::box_value(patchbay::DescribeFrameRate(each, true)));
                }

                rate.SelectedIndex(static_cast<int32_t>(timeCode.FrameRate));

                controls::TextBox start{};

                start.Header(winrt::box_value(resources::GetString(L"GeneratorStartAt")));
                start.PlaceholderText(L"00:00:00:00");
                start.MaxLength(16);
                start.Text(winrt::hstring{ midiapp::FormatPosition(timeCode.Start, timeCode.FrameRate) });

                auto startText = Caption();
                startText.FontSize(12);
                startText.Text(resources::FormatString(L"GeneratorStartReadingFormat",
                    midiapp::FormatPosition(timeCode.Start, timeCode.FrameRate)));

                // What doesn't read as a time is left alone, and the line under it says how to
                // write one. Kept when the box is left, not on every key.
                auto const readStart = [weak, blockId, start, startText](bool keep)
                    {
                        auto strong = weak.get();
                        auto const* patch = strong == nullptr ? nullptr : strong->CurrentPatch();
                        auto const* target = patch == nullptr ? nullptr : patch->FindBlock(blockId);

                        if (target == nullptr)
                        {
                            return;
                        }

                        auto const frameRate = target->Settings.TimeCode.FrameRate;
                        midiapp::MidiTimeCodePosition position{};

                        if (!midiapp::TryParsePosition(std::wstring_view{ start.Text() }, frameRate, position))
                        {
                            startText.Text(resources::GetString(L"GeneratorStartNotUnderstood"));
                            return;
                        }

                        position = midiapp::ClampPosition(position, frameRate);

                        startText.Text(resources::FormatString(L"GeneratorStartReadingFormat",
                            midiapp::FormatPosition(position, frameRate)));

                        if (keep)
                        {
                            strong->ChangeStepSettings(blockId, [position](patchbay::BlockSettings& s) { s.TimeCode.Start = position; });
                        }
                    };

                start.TextChanged([readStart](auto&&, auto&&) { readStart(false); });
                start.LostFocus([readStart](auto&&, auto&&) { readStart(true); });

                start.KeyDown([readStart](auto&&, input::KeyRoutedEventArgs const& args)
                    {
                        if (args.Key() == winrt::Windows::System::VirtualKey::Enter)
                        {
                            args.Handled(true);
                            readStart(true);
                        }
                    });

                rate.SelectionChanged([weak, change, start](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo == nullptr || combo.SelectedIndex() < 0)
                        {
                            return;
                        }

                        auto const frameRate = midiapp::FrameRateFromValue(combo.SelectedIndex());

                        // A frame number can be too high for the new rate, or one 29.97 skips.
                        if (auto const changed = change([frameRate](patchbay::BlockSettings& s)
                            {
                                s.TimeCode.FrameRate = frameRate;
                                s.TimeCode.Start = midiapp::ClampPosition(s.TimeCode.Start, frameRate);
                            }))
                        {
                            if (auto strong = weak.get())
                            {
                                strong->m_updatingStepSettings = true;
                                start.Text(winrt::hstring{ midiapp::FormatPosition(changed->TimeCode.Start, frameRate) });
                                strong->m_updatingStepSettings = false;
                            }
                        }
                    });

                body.Children().Append(rate);
                body.Children().Append(start);
                body.Children().Append(startText);

                auto fullFrame = Check(resources::GetString(L"GeneratorSendFullFrame"), timeCode.SendFullFrame);

                auto const setFullFrame = [change](bool on)
                    {
                        change([on](patchbay::BlockSettings& s) { s.TimeCode.SendFullFrame = on; });
                    };

                fullFrame.Checked([setFullFrame](auto&&, auto&&) { setFullFrame(true); });
                fullFrame.Unchecked([setFullFrame](auto&&, auto&&) { setFullFrame(false); });

                body.Children().Append(fullFrame);
                body.Children().Append(Hint(resources::GetString(L"GeneratorSendFullFrameHint")));

                auto group = GroupChoices(timeCode.Group);

                group.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            auto const value = static_cast<uint8_t>(combo.SelectedIndex());
                            change([value](patchbay::BlockSettings& s) { s.TimeCode.Group = value; });
                        }
                    });

                body.Children().Append(group);
                body.Children().Append(Hint(resources::GetString(L"GeneratorGroupHint")));

                m_stepSettingsFocus = rate;
                break;
            }

            case patchbay::BlockKind::NoteDistributor:
            {
                auto const& distributor = settings.Distributor;

                controls::ComboBox mode{};

                mode.Header(winrt::box_value(resources::GetString(L"DistributorMode")));
                mode.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);

                for (auto const key : { L"DistributorModeTurns", L"DistributorModeFirstFree",
                                        L"DistributorModeHighest", L"DistributorModeLowest" })
                {
                    mode.Items().Append(winrt::box_value(resources::GetString(key)));
                }

                mode.SelectedIndex(static_cast<int32_t>(distributor.Mode));

                auto modeHint = Hint(resources::GetString(L"DistributorModeHint"));

                mode.SelectionChanged([change](foundation::IInspectable const& sender, auto&&)
                    {
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            auto const value = static_cast<patchbay::DistributionMode>(combo.SelectedIndex());
                            change([value](patchbay::BlockSettings& s) { s.Distributor.Mode = value; });
                        }
                    });

                body.Children().Append(mode);
                body.Children().Append(modeHint);

                // A voice is a connection out, in the order they were made.
                size_t voices{ 0 };

                if (auto const* patch = CurrentPatch())
                {
                    voices = static_cast<size_t>(std::count_if(patch->Connections.begin(), patch->Connections.end(),
                        [&blockId](patchbay::PatchConnection const& link) { return link.SourceId == blockId && !link.Muted; }));
                }

                auto count = Caption();
                count.FontSize(12);
                count.Text(voices == 1
                    ? resources::GetString(L"DistributorVoicesOne")
                    : resources::FormatString(L"DistributorVoicesFormat", static_cast<int>(voices)));

                body.Children().Append(count);

                struct Option
                {
                    wchar_t const* Key;
                    bool Value;
                    bool patchbay::NoteDistributorSettings::* Field;
                };

                for (auto const& option : {
                    Option{ L"DistributorControlChangesToAll", distributor.ControlChangesToEveryVoice, &patchbay::NoteDistributorSettings::ControlChangesToEveryVoice },
                    Option{ L"DistributorChannelPressureToAll", distributor.ChannelPressureToEveryVoice, &patchbay::NoteDistributorSettings::ChannelPressureToEveryVoice },
                    Option{ L"DistributorPitchBendToAll", distributor.PitchBendToEveryVoice, &patchbay::NoteDistributorSettings::PitchBendToEveryVoice } })
                {
                    auto check = Check(resources::GetString(option.Key), option.Value);
                    auto const field = option.Field;

                    auto const set = [change, field](bool on)
                        {
                            change([field, on](patchbay::BlockSettings& s) { s.Distributor.*field = on; });
                        };

                    check.Checked([set](auto&&, auto&&) { set(true); });
                    check.Unchecked([set](auto&&, auto&&) { set(false); });

                    body.Children().Append(check);
                }

                body.Children().Append(Hint(resources::GetString(L"DistributorToAllHint")));

                m_stepSettingsFocus = mode;
                break;
            }

            case patchbay::BlockKind::CiResponder:
                BuildCiResponderSettings(block, body);
                break;

            case patchbay::BlockKind::CiFilter:
                BuildCiFilterSettings(block, body);
                break;

            default:
                break;
            }
        }
        catch (...)
        {
            m_updatingStepSettings = false;
            MIDI_PATCHBAY_LOG_GENERAL_EXCEPTION(L"Unable to build the step's settings.");
        }
    }
}
