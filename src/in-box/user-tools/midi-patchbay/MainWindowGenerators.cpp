// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

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
        controls::TextBlock GeneratorHeading(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(13);
            block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

            return block;
        }

        controls::TextBlock GeneratorHint(_In_ winrt::hstring const& text) noexcept
        {
            controls::TextBlock block{};

            block.Text(text);
            block.FontSize(11);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Margin(xaml::ThicknessHelper::FromLengths(0, -4, 0, 4));
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

            return block;
        }

        // A line that follows a setting, such as how long a sweep takes at this tempo.
        controls::TextBlock GeneratorCaption() noexcept
        {
            controls::TextBlock block{};

            block.FontSize(12);
            block.TextWrapping(xaml::TextWrapping::Wrap);
            block.Margin(xaml::ThicknessHelper::FromLengths(0, 4, 0, 0));
            block.Foreground(patchbay::ThemeBrushes::Current().Get(L"TextFillColorSecondaryBrush"));

            return block;
        }

        controls::Border GeneratorCard(_In_ xaml::UIElement const& content) noexcept
        {
            controls::Border card{};

            card.CornerRadius(xaml::CornerRadiusHelper::FromUniformRadius(6));
            card.BorderThickness(xaml::ThicknessHelper::FromUniformLength(1));
            card.Padding(xaml::ThicknessHelper::FromLengths(12, 10, 12, 12));
            card.BorderBrush(patchbay::ThemeBrushes::Current().Get(L"CardStrokeColorDefaultBrush"));
            card.Background(patchbay::ThemeBrushes::Current().Get(L"CardBackgroundFillColorSecondaryBrush"));
            card.Child(content);

            return card;
        }

        controls::NumberBox GeneratorNumberBox(
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
            box.Width(150);

            // Inline, not Compact: the compact spin buttons live in a popup that the dialog's
            // scroll viewer does not clip.
            box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
            box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
            box.Value(value);

            return box;
        }

        // Sixteen choices counted from 1 on screen and stored from 0, for a group or a channel.
        controls::ComboBox SixteenChoices(
            _In_ winrt::hstring const& header,
            _In_ wchar_t const* itemFormat,
            _In_ int32_t selected) noexcept
        {
            controls::ComboBox box{};

            box.Header(winrt::box_value(header));
            box.MinWidth(150);

            for (int32_t i = 0; i < 16; i++)
            {
                box.Items().Append(winrt::box_value(resources::FormatString(itemFormat, i + 1)));
            }

            box.SelectedIndex(std::clamp(selected, 0, 15));

            return box;
        }

        controls::StackPanel GeneratorRow() noexcept
        {
            controls::StackPanel row{};

            row.Orientation(controls::Orientation::Horizontal);
            row.Spacing(16);

            return row;
        }

        // Two places, the same as a summary shows.
        double RoundedTempo(_In_ double value) noexcept
        {
            return std::round(std::clamp(value,
                patchbay::MinimumGeneratorBeatsPerMinute,
                patchbay::MaximumGeneratorBeatsPerMinute) * 100.0) / 100.0;
        }

        // What an LFO's number means for each kind, so changing the kind only resets it when the
        // old number would mean something else.
        int32_t NumberMeaning(_In_ midiapp::ValueMessageKind kind) noexcept
        {
            switch (kind)
            {
            case midiapp::ValueMessageKind::ControlChange:          return 1;
            case midiapp::ValueMessageKind::PolyPressure:           return 2;
            case midiapp::ValueMessageKind::RegisteredController:
            case midiapp::ValueMessageKind::AssignableController:   return 3;
            default:                                                return 0;
            }
        }
    }

    void MainWindow::BuildClockGeneratorSections() noexcept
    {
        try
        {
            auto weak = get_weak();
            auto const content = BlockDialogContent();
            auto const& clock = m_editingSettings.Clock;

            // ------------------------------------------------------------ tempo
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionTempo")));
                body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorClockTempoHint")));

                auto tempo = GeneratorNumberBox(resources::GetString(L"GeneratorBeatsPerMinute"),
                    patchbay::MinimumGeneratorBeatsPerMinute, patchbay::MaximumGeneratorBeatsPerMinute,
                    clock.BeatsPerMinute);

                tempo.Width(220);

                tempo.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        s->m_editingSettings.Clock.BeatsPerMinute = RoundedTempo(args.NewValue());
                        s->UpdateBlockSummary();
                    });

                body.Children().Append(tempo);

                controls::CheckBox startStop{};

                startStop.Content(winrt::box_value(resources::GetString(L"GeneratorSendStartStop")));
                startStop.IsChecked(clock.SendStartStop);
                startStop.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));

                auto const setStartStop = [weak](bool value)
                    {
                        if (auto s = weak.get())
                        {
                            s->m_editingSettings.Clock.SendStartStop = value;
                            s->UpdateBlockSummary();
                        }
                    };

                startStop.Checked([setStartStop](auto&&, auto&&) { setStartStop(true); });
                startStop.Unchecked([setStartStop](auto&&, auto&&) { setStartStop(false); });

                body.Children().Append(startStop);
                body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorSendStartStopHint")));

                content.Children().Append(GeneratorCard(body));
            }

            // ------------------------------------------------------------ swing
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionSwing")));

                auto row = GeneratorRow();

                auto swing = GeneratorNumberBox(resources::GetString(L"GeneratorSwingPercent"), 50, 75, clock.SwingPercent);

                swing.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        s->m_editingSettings.Clock.SwingPercent = std::round(std::clamp(args.NewValue(), 50.0, 75.0));
                        s->RefreshGeneratorCaptions();
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(swing);

                controls::ComboBox subdivision{};

                subdivision.Header(winrt::box_value(resources::GetString(L"GeneratorSwingAppliesTo")));
                subdivision.MinWidth(180);
                subdivision.Items().Append(winrt::box_value(resources::GetString(L"GeneratorSwingEighths")));
                subdivision.Items().Append(winrt::box_value(resources::GetString(L"GeneratorSwingSixteenths")));
                subdivision.SelectedIndex(clock.SwingSubdivision == 4 ? 1 : 0);

                subdivision.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0)
                        {
                            return;
                        }

                        s->m_editingSettings.Clock.SwingSubdivision = combo.SelectedIndex() == 1 ? 4 : 2;
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(subdivision);
                body.Children().Append(row);

                m_swingCaption = GeneratorCaption();
                body.Children().Append(m_swingCaption);

                content.Children().Append(GeneratorCard(body));
            }

            content.Children().Append(BuildGeneratorGroupCard(&m_editingSettings.Clock.Group));

            RefreshGeneratorCaptions();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the clock sections.")
    }

    void MainWindow::BuildTimeCodeGeneratorSections() noexcept
    {
        try
        {
            auto weak = get_weak();
            auto const content = BlockDialogContent();
            auto const& timeCode = m_editingSettings.TimeCode;

            controls::StackPanel body{};
            body.Spacing(4);

            body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionTimeCode")));
            body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorTimeCodeHint")));

            auto row = GeneratorRow();

            // In the order of the format's own rate codes, so the index is the code.
            controls::ComboBox rate{};

            rate.Header(winrt::box_value(resources::GetString(L"GeneratorFrameRate")));
            rate.MinWidth(280);

            for (auto const each : { midiapp::MidiTimeCodeFrameRate::Frames24, midiapp::MidiTimeCodeFrameRate::Frames25,
                                     midiapp::MidiTimeCodeFrameRate::Frames2997Drop, midiapp::MidiTimeCodeFrameRate::Frames30 })
            {
                rate.Items().Append(winrt::box_value(patchbay::DescribeFrameRate(each, true)));
            }

            rate.SelectedIndex(static_cast<int32_t>(timeCode.FrameRate));

            rate.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const combo = sender.try_as<controls::ComboBox>();

                    if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0)
                    {
                        return;
                    }

                    auto& settings = s->m_editingSettings.TimeCode;

                    settings.FrameRate = midiapp::FrameRateFromValue(combo.SelectedIndex());

                    // A frame number can be too high for the new rate, or one 29.97 skips.
                    settings.Start = midiapp::ClampPosition(settings.Start, settings.FrameRate);

                    s->ApplyStartTimeText();
                    s->UpdateBlockSummary();
                });

            row.Children().Append(rate);

            m_startTimeBox = controls::TextBox{};
            m_startTimeBox.Header(winrt::box_value(resources::GetString(L"GeneratorStartAt")));
            m_startTimeBox.PlaceholderText(L"00:00:00:00");
            m_startTimeBox.Width(150);
            m_startTimeBox.MaxLength(16);
            m_startTimeBox.Text(winrt::hstring{ midiapp::FormatPosition(timeCode.Start, timeCode.FrameRate) });

            m_startTimeBox.TextChanged([weak](auto&&, auto&&)
                {
                    if (auto s = weak.get())
                    {
                        s->ApplyStartTimeText();
                        s->UpdateBlockSummary();
                    }
                });

            row.Children().Append(m_startTimeBox);
            body.Children().Append(row);

            m_startTimeCaption = GeneratorCaption();
            body.Children().Append(m_startTimeCaption);

            controls::CheckBox fullFrame{};

            fullFrame.Content(winrt::box_value(resources::GetString(L"GeneratorSendFullFrame")));
            fullFrame.IsChecked(timeCode.SendFullFrame);
            fullFrame.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));

            auto const setFullFrame = [weak](bool value)
                {
                    if (auto s = weak.get())
                    {
                        s->m_editingSettings.TimeCode.SendFullFrame = value;
                        s->UpdateBlockSummary();
                    }
                };

            fullFrame.Checked([setFullFrame](auto&&, auto&&) { setFullFrame(true); });
            fullFrame.Unchecked([setFullFrame](auto&&, auto&&) { setFullFrame(false); });

            body.Children().Append(fullFrame);
            body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorSendFullFrameHint")));

            content.Children().Append(GeneratorCard(body));
            content.Children().Append(BuildGeneratorGroupCard(&m_editingSettings.TimeCode.Group));

            ApplyStartTimeText();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the time code sections.")
    }

    // What doesn't read as a time keeps the last start that did, and says how to write one.
    void MainWindow::ApplyStartTimeText() noexcept
    {
        try
        {
            if (m_startTimeBox == nullptr || m_startTimeCaption == nullptr)
            {
                return;
            }

            auto& settings = m_editingSettings.TimeCode;
            midiapp::MidiTimeCodePosition position{};

            if (midiapp::TryParsePosition(std::wstring_view{ m_startTimeBox.Text() }, settings.FrameRate, position))
            {
                settings.Start = midiapp::ClampPosition(position, settings.FrameRate);

                m_startTimeCaption.Text(resources::FormatString(L"GeneratorStartReadingFormat",
                    midiapp::FormatPosition(settings.Start, settings.FrameRate)));
            }
            else
            {
                m_startTimeCaption.Text(resources::GetString(L"GeneratorStartNotUnderstood"));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read the start time.")
    }

    void MainWindow::BuildLfoGeneratorSections() noexcept
    {
        try
        {
            auto weak = get_weak();
            auto const content = BlockDialogContent();
            auto const& lfo = m_editingSettings.Lfo;

            // ---------------------------------------------------- shape and rate
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionWave")));

                auto row = GeneratorRow();

                controls::ComboBox wave{};

                wave.Header(winrt::box_value(resources::GetString(L"GeneratorWave")));
                wave.MinWidth(170);

                for (size_t i = 0; i < std::size(midiapp::LfoWaveOrder); i++)
                {
                    wave.Items().Append(winrt::box_value(patchbay::DescribeLfoWave(midiapp::LfoWaveOrder[i])));

                    if (midiapp::LfoWaveOrder[i] == lfo.Wave)
                    {
                        wave.SelectedIndex(static_cast<int32_t>(i));
                    }
                }

                wave.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0 ||
                            static_cast<size_t>(combo.SelectedIndex()) >= std::size(midiapp::LfoWaveOrder))
                        {
                            return;
                        }

                        s->m_editingSettings.Lfo.Wave = midiapp::LfoWaveOrder[combo.SelectedIndex()];
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(wave);

                // The musical lengths, and whatever else a file asked for.
                std::vector<double> lengths(std::begin(midiapp::LfoRateChoices), std::end(midiapp::LfoRateChoices));

                if (std::none_of(lengths.begin(), lengths.end(),
                    [&lfo](double length) { return std::abs(length - lfo.BeatsPerCycle) < 0.0001; }))
                {
                    lengths.push_back(lfo.BeatsPerCycle);
                }

                controls::ComboBox rate{};

                rate.Header(winrt::box_value(resources::GetString(L"GeneratorRate")));
                rate.MinWidth(150);

                for (size_t i = 0; i < lengths.size(); i++)
                {
                    rate.Items().Append(winrt::box_value(patchbay::DescribeLfoLength(lengths[i])));

                    if (std::abs(lengths[i] - lfo.BeatsPerCycle) < 0.0001)
                    {
                        rate.SelectedIndex(static_cast<int32_t>(i));
                    }
                }

                rate.SelectionChanged([weak, lengths](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0 ||
                            static_cast<size_t>(combo.SelectedIndex()) >= lengths.size())
                        {
                            return;
                        }

                        s->m_editingSettings.Lfo.BeatsPerCycle = lengths[static_cast<size_t>(combo.SelectedIndex())];
                        s->RefreshGeneratorCaptions();
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(rate);

                auto tempo = GeneratorNumberBox(resources::GetString(L"GeneratorBeatsPerMinute"),
                    patchbay::MinimumGeneratorBeatsPerMinute, patchbay::MaximumGeneratorBeatsPerMinute,
                    lfo.BeatsPerMinute);

                tempo.Width(220);

                tempo.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        s->m_editingSettings.Lfo.BeatsPerMinute = RoundedTempo(args.NewValue());
                        s->RefreshGeneratorCaptions();
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(tempo);
                body.Children().Append(row);

                m_lfoRateCaption = GeneratorCaption();
                body.Children().Append(m_lfoRateCaption);

                content.Children().Append(GeneratorCard(body));
            }

            // ------------------------------------------------------------ range
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionRange")));
                body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorRangeHint")));

                auto row = GeneratorRow();

                auto const addEnd = [weak, &row](winrt::hstring const& header, int32_t hundredths, bool isLowest)
                    {
                        auto box = GeneratorNumberBox(header, 0, 100,
                            patchbay::DisplayFromHundredths(hundredths, patchbay::ValueScale::Percent));

                        box.ValueChanged([weak, isLowest](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto const value = std::clamp(
                                    patchbay::HundredthsFromDisplay(args.NewValue(), patchbay::ValueScale::Percent),
                                    0, patchbay::FullScaleHundredths);

                                (isLowest ? s->m_editingSettings.Lfo.LowestHundredths : s->m_editingSettings.Lfo.HighestHundredths) = value;
                                s->UpdateBlockSummary();
                            });

                        row.Children().Append(box);
                    };

                addEnd(resources::GetString(L"GeneratorLowest"), lfo.LowestHundredths, true);
                addEnd(resources::GetString(L"GeneratorHighest"), lfo.HighestHundredths, false);

                body.Children().Append(row);

                content.Children().Append(GeneratorCard(body));
            }

            // ---------------------------------------------------- what it sends
            {
                auto const& target = lfo.Target;

                controls::StackPanel body{};
                body.Spacing(8);

                body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionMessage")));

                auto row = GeneratorRow();

                controls::ComboBox kind{};

                kind.Header(winrt::box_value(resources::GetString(L"GeneratorMessage")));
                kind.MinWidth(280);

                for (size_t i = 0; i < std::size(midiapp::AllValueMessageKinds); i++)
                {
                    kind.Items().Append(winrt::box_value(patchbay::DescribeValueMessageKind(midiapp::AllValueMessageKinds[i])));

                    if (midiapp::AllValueMessageKinds[i] == target.Kind)
                    {
                        kind.SelectedIndex(static_cast<int32_t>(i));
                    }
                }

                kind.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s == nullptr || combo == nullptr || combo.SelectedIndex() < 0 ||
                            static_cast<size_t>(combo.SelectedIndex()) >= std::size(midiapp::AllValueMessageKinds))
                        {
                            return;
                        }

                        auto& settings = s->m_editingSettings.Lfo.Target;
                        auto const chosen = midiapp::AllValueMessageKinds[combo.SelectedIndex()];

                        if (chosen == settings.Kind)
                        {
                            return;
                        }

                        if (NumberMeaning(chosen) != NumberMeaning(settings.Kind))
                        {
                            settings.Number = patchbay::DefaultLfoNumber(chosen);
                        }

                        settings.Kind = chosen;

                        s->RefreshLfoMessageUi();
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(kind);

                // Only the boxes the kind uses are shown. Each ignores a change while it is hidden,
                // because the others share the one number.
                m_lfoNumberBox = GeneratorNumberBox(resources::GetString(L"GeneratorController"), 0, 127, 0);

                m_lfoNumberBox.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        auto& settings = s->m_editingSettings.Lfo.Target;

                        if (midiapp::ValueMessageNumberMaximum(settings.Kind) != 127)
                        {
                            return;
                        }

                        settings.Number = static_cast<uint32_t>(std::clamp(args.NewValue(), 0.0, 127.0));
                        s->UpdateBlockSummary();
                    });

                row.Children().Append(m_lfoNumberBox);

                auto const addBankPart = [weak, &row](winrt::hstring const& header, bool isBank)
                    {
                        auto box = GeneratorNumberBox(header, 0, 127, 0);

                        box.ValueChanged([weak, isBank](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                            {
                                auto s = weak.get();

                                if (s == nullptr || std::isnan(args.NewValue()))
                                {
                                    return;
                                }

                                auto& settings = s->m_editingSettings.Lfo.Target;

                                if (midiapp::ValueMessageNumberMaximum(settings.Kind) <= 127)
                                {
                                    return;
                                }

                                auto const part = static_cast<uint32_t>(std::clamp(args.NewValue(), 0.0, 127.0));
                                auto bank = (settings.Number >> 7) & 0x7F;
                                auto index = settings.Number & 0x7F;

                                (isBank ? bank : index) = part;

                                settings.Number = (bank << 7) | index;
                                s->UpdateBlockSummary();
                            });

                        row.Children().Append(box);

                        return box;
                    };

                m_lfoBankBox = addBankPart(resources::GetString(L"GeneratorBank"), true);
                m_lfoIndexBox = addBankPart(resources::GetString(L"GeneratorIndex"), false);

                body.Children().Append(row);

                auto where = GeneratorRow();

                auto channel = SixteenChoices(resources::GetString(L"GeneratorChannel"), L"FilterChannelFormat", target.Channel);

                channel.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s != nullptr && combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            s->m_editingSettings.Lfo.Target.Channel = static_cast<uint8_t>(combo.SelectedIndex());
                            s->UpdateBlockSummary();
                        }
                    });

                where.Children().Append(channel);

                auto group = SixteenChoices(resources::GetString(L"GeneratorGroup"), L"FilterGroupFormat", target.Group);

                group.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const combo = sender.try_as<controls::ComboBox>();

                        if (s != nullptr && combo != nullptr && combo.SelectedIndex() >= 0)
                        {
                            s->m_editingSettings.Lfo.Target.Group = static_cast<uint8_t>(combo.SelectedIndex());
                            s->UpdateBlockSummary();
                        }
                    });

                where.Children().Append(group);
                body.Children().Append(where);

                m_lfoProtocolButtons = controls::RadioButtons{};

                m_lfoProtocolButtons.Header(winrt::box_value(resources::GetString(L"GeneratorProtocol")));
                m_lfoProtocolButtons.MaxColumns(2);
                m_lfoProtocolButtons.Items().Append(winrt::box_value(resources::GetString(L"GeneratorProtocolMidi2")));
                m_lfoProtocolButtons.Items().Append(winrt::box_value(resources::GetString(L"GeneratorProtocolMidi1")));
                m_lfoProtocolButtons.SelectedIndex(target.Midi1Protocol ? 1 : 0);

                m_lfoProtocolButtons.SelectionChanged([weak](foundation::IInspectable const& sender, auto&&)
                    {
                        auto s = weak.get();
                        auto const list = sender.try_as<controls::RadioButtons>();

                        if (s != nullptr && list != nullptr && list.SelectedIndex() >= 0)
                        {
                            s->m_editingSettings.Lfo.Target.Midi1Protocol = list.SelectedIndex() == 1;
                            s->UpdateBlockSummary();
                        }
                    });

                body.Children().Append(m_lfoProtocolButtons);
                body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorProtocolHint")));

                content.Children().Append(GeneratorCard(body));
            }

            // ------------------------------------------------ how often, and stopping
            {
                controls::StackPanel body{};
                body.Spacing(4);

                body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionInterval")));

                auto interval = GeneratorNumberBox(resources::GetString(L"GeneratorInterval"),
                    midiapp::MinimumLfoIntervalMilliseconds, midiapp::MaximumLfoIntervalMilliseconds,
                    lfo.IntervalMilliseconds);

                interval.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                    {
                        auto s = weak.get();

                        if (s == nullptr || std::isnan(args.NewValue()))
                        {
                            return;
                        }

                        s->m_editingSettings.Lfo.IntervalMilliseconds = static_cast<int32_t>(std::lround(std::clamp(args.NewValue(),
                            static_cast<double>(midiapp::MinimumLfoIntervalMilliseconds),
                            static_cast<double>(midiapp::MaximumLfoIntervalMilliseconds))));

                        s->RefreshGeneratorCaptions();
                        s->UpdateBlockSummary();
                    });

                body.Children().Append(interval);

                m_lfoIntervalCaption = GeneratorCaption();
                body.Children().Append(m_lfoIntervalCaption);

                controls::CheckBox center{};

                center.Content(winrt::box_value(resources::GetString(L"GeneratorReturnToCenter")));
                center.IsChecked(lfo.ReturnsToMiddle);
                center.Margin(xaml::ThicknessHelper::FromLengths(0, 8, 0, 0));

                auto const setCenter = [weak](bool value)
                    {
                        if (auto s = weak.get())
                        {
                            s->m_editingSettings.Lfo.ReturnsToMiddle = value;
                            s->UpdateBlockSummary();
                        }
                    };

                center.Checked([setCenter](auto&&, auto&&) { setCenter(true); });
                center.Unchecked([setCenter](auto&&, auto&&) { setCenter(false); });

                body.Children().Append(center);
                body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorReturnToCenterHint")));

                content.Children().Append(GeneratorCard(body));
            }

            RefreshLfoMessageUi();
            RefreshGeneratorCaptions();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the LFO sections.")
    }

    void MainWindow::RefreshLfoMessageUi() noexcept
    {
        try
        {
            auto const& target = m_editingSettings.Lfo.Target;
            auto const maximum = midiapp::ValueMessageNumberMaximum(target.Kind);

            auto const numbered = maximum == 127;
            auto const banked = maximum > 127;

            if (m_lfoNumberBox != nullptr)
            {
                m_lfoNumberBox.Header(winrt::box_value(resources::GetString(
                    target.Kind == midiapp::ValueMessageKind::PolyPressure ? L"GeneratorNote" : L"GeneratorController")));
                m_lfoNumberBox.Visibility(numbered ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

                if (numbered)
                {
                    m_lfoNumberBox.Value(target.Number);
                }
            }

            for (auto const& box : { m_lfoBankBox, m_lfoIndexBox })
            {
                if (box != nullptr)
                {
                    box.Visibility(banked ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                }
            }

            if (banked && m_lfoBankBox != nullptr && m_lfoIndexBox != nullptr)
            {
                m_lfoBankBox.Value((target.Number >> 7) & 0x7F);
                m_lfoIndexBox.Value(target.Number & 0x7F);
            }

            // RPN and NRPN only go as MIDI 2.0.
            if (m_lfoProtocolButtons != nullptr)
            {
                m_lfoProtocolButtons.IsEnabled(!banked);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the LFO message settings.")
    }

    void MainWindow::BuildClockDividerSection() noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(4);

            body.Children().Append(GeneratorHeading(resources::GetString(L"DividerSection")));
            body.Children().Append(GeneratorHint(resources::GetString(L"DividerHint")));

            auto divideBy = GeneratorNumberBox(resources::GetString(L"DividerDivideBy"),
                1, patchbay::MaximumClockDivision, m_editingSettings.ClockDivision);

            divideBy.ValueChanged([weak](auto&&, controls::NumberBoxValueChangedEventArgs const& args)
                {
                    auto s = weak.get();

                    if (s == nullptr || std::isnan(args.NewValue()))
                    {
                        return;
                    }

                    s->m_editingSettings.ClockDivision = static_cast<uint32_t>(std::lround(std::clamp(args.NewValue(),
                        1.0, static_cast<double>(patchbay::MaximumClockDivision))));

                    s->RefreshGeneratorCaptions();
                    s->UpdateBlockSummary();
                });

            body.Children().Append(divideBy);

            m_dividerCaption = GeneratorCaption();
            body.Children().Append(m_dividerCaption);

            BlockDialogContent().Children().Append(GeneratorCard(body));

            RefreshGeneratorCaptions();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the clock divider section.")
    }

    _Use_decl_annotations_
    controls::Border MainWindow::BuildGeneratorGroupCard(uint8_t* group) noexcept
    {
        try
        {
            auto weak = get_weak();

            controls::StackPanel body{};
            body.Spacing(4);

            body.Children().Append(GeneratorHeading(resources::GetString(L"GeneratorSectionGroup")));
            body.Children().Append(GeneratorHint(resources::GetString(L"GeneratorGroupHint")));

            auto choices = SixteenChoices(resources::GetString(L"GeneratorGroup"), L"FilterGroupFormat", *group);

            // The pointer is into the dialog's working copy, which lives as long as the window.
            choices.SelectionChanged([weak, group](foundation::IInspectable const& sender, auto&&)
                {
                    auto s = weak.get();
                    auto const combo = sender.try_as<controls::ComboBox>();

                    if (s != nullptr && combo != nullptr && combo.SelectedIndex() >= 0)
                    {
                        *group = static_cast<uint8_t>(combo.SelectedIndex());
                        s->UpdateBlockSummary();
                    }
                });

            body.Children().Append(choices);

            return GeneratorCard(body);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build the group section.")

        return controls::Border{};
    }

    void MainWindow::RefreshGeneratorCaptions() noexcept
    {
        try
        {
            if (m_swingCaption != nullptr)
            {
                auto const swing = static_cast<int>(std::lround(m_editingSettings.Clock.SwingPercent));

                m_swingCaption.Text(swing <= 50
                    ? resources::GetString(L"GeneratorSwingStraight")
                    : resources::FormatString(L"GeneratorSwingValueFormat", swing));
            }

            if (m_lfoRateCaption != nullptr)
            {
                auto const& lfo = m_editingSettings.Lfo;
                auto const seconds = lfo.BeatsPerCycle * 60.0 / (std::max)(lfo.BeatsPerMinute, 1.0);

                m_lfoRateCaption.Text(resources::FormatString(L"GeneratorLfoRateCaptionFormat",
                    patchbay::DescribeNumber(lfo.BeatsPerCycle, 3),
                    patchbay::DescribeNumber(seconds, 2),
                    patchbay::DescribeTempo(lfo.BeatsPerMinute)));
            }

            if (m_lfoIntervalCaption != nullptr)
            {
                auto const perSecond = static_cast<int>(std::lround(
                    1000.0 / (std::max)(m_editingSettings.Lfo.IntervalMilliseconds, 1)));

                m_lfoIntervalCaption.Text(resources::FormatString(L"GeneratorIntervalCaptionFormat", perSecond));
            }

            if (m_dividerCaption != nullptr)
            {
                auto const division = (std::max)(m_editingSettings.ClockDivision, 1u);

                m_dividerCaption.Text(resources::FormatString(L"DividerEffectFormat",
                    patchbay::DescribeNumber(120.0 / division, 2)));
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to update the step's captions.")
    }
}
