// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The step sequencer's panel in the inspector: how fast and which way it plays, how long each
// note sounds, and the steps themselves, one row each.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "PadGrid.h"
#include "StepPattern.h"

#include <format>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr wchar_t const* StepRateKeys[]
        {
            L"StepsRateQuarter", L"StepsRateEighth", L"StepsRateEighthTriplet",
            L"StepsRateSixteenth", L"StepsRateSixteenthTriplet", L"StepsRateThirtySecond",
        };

        static_assert(std::size(glass::StepRateChoices) == std::size(StepRateKeys));

        constexpr wchar_t const* StepDirectionKeys[]
        {
            L"StepsDirectionForward", L"StepsDirectionBackward", L"StepsDirectionPingPong",
            L"StepsDirectionRandom",
        };

        static_assert(std::size(glass::StepDirectionOrder) == std::size(StepDirectionKeys));

        // Velocity is shown from 1 to 100. Zero is left off the scale on purpose: a step that
        // plays at nothing is a rest, and there is a box for that.
        constexpr double LowestVelocityPercent = 1.0;
        constexpr double HighestVelocityPercent = 100.0;

        void FillStepRow(
            _In_ controls::CheckBox const& plays,
            _In_ controls::NumberBox const& note,
            _In_ controls::TextBlock const& noteName,
            _In_ controls::NumberBox const& velocity,
            _In_ glass::SequencerStep const& step)
        {
            auto const clamped = std::clamp(step.Note, 0, 127);
            auto const name = winrt::hstring{ glass::PadNoteName(clamped, false) };

            plays.IsChecked(step.On);
            note.Value(clamped);
            noteName.Text(name);

            // The name beside the box is drawn text only, so a screen reader hears it here.
            xaml::Automation::AutomationProperties::SetHelpText(note, name);

            velocity.Value(std::clamp(std::round(step.Velocity * 100.0), LowestVelocityPercent, HighestVelocityPercent));
        }
    }

    void EditorWindow::BuildStepsChoices()
    {
        for (auto const* const key : StepRateKeys)
        {
            StepsRateCombo().Items().Append(box_value(resources::GetString(key)));
        }

        for (auto const* const key : StepDirectionKeys)
        {
            StepsDirectionCombo().Items().Append(box_value(resources::GetString(key)));
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshStepsPanel(glass::Control const& control)
    {
        auto const& spec = control.Steps;

        // A file can carry a rate that is not on the list. The combo is left blank rather than
        // snapped to the nearest, and the caption below says what is really set.
        auto rateIndex = -1;

        for (size_t index = 0; index < std::size(glass::StepRateChoices); ++index)
        {
            if (std::abs(spec.StepsPerBeat - glass::StepRateChoices[index]) < 0.0001)
            {
                rateIndex = static_cast<int32_t>(index);
                break;
            }
        }

        StepsRateCombo().SelectedIndex(rateIndex);

        auto directionIndex = 0;

        for (size_t index = 0; index < std::size(glass::StepDirectionOrder); ++index)
        {
            if (glass::StepDirectionOrder[index] == spec.Direction)
            {
                directionIndex = static_cast<int32_t>(index);
                break;
            }
        }

        StepsDirectionCombo().SelectedIndex(directionIndex);

        StepsGateSlider().Value(std::round(spec.Gate * 100.0));
        StepsSwingSlider().Value(std::round(spec.Swing * 100.0));

        auto const tempo = std::clamp(
            m_editor.Document().Tempo.BeatsPerMinute,
            glass::MinimumBeatsPerMinute,
            glass::MaximumBeatsPerMinute);

        StepsTimingCaption().Text(winrt::hstring{ resources::FormatString(
            L"StepsTimingCaptionFormat",
            std::format(L"{:.0f}", glass::StepMicroseconds(spec, tempo) / 1000.0),
            std::format(L"{:.0f}", tempo)) });

        StepsLatchingCheck().IsChecked(spec.Latching);
        StepsStartsRunningCheck().IsChecked(spec.StartsRunning);
        StepCountBox().Value(glass::SequencerStepCount(spec));

        RefreshStepRows(control);
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshStepRows(glass::Control const& control)
    {
        auto const previous = m_updatingInspector;

        m_updatingInspector = true;

        auto restore = wil::scope_exit([this, previous]() { m_updatingInspector = previous; });

        try
        {
            auto const count = static_cast<size_t>(glass::SequencerStepCount(control.Steps));
            auto const rows = StepRows();

            // The same control with the same number of steps: the numbers change where they are,
            // so a spin button being held or a box being typed in keeps the keyboard.
            if (m_stepRowsControlId == control.Id &&
                m_stepRows.size() == count &&
                rows.Children().Size() == count)
            {
                for (size_t index = 0; index < count; ++index)
                {
                    auto const& row = m_stepRows[index];

                    FillStepRow(row.Plays, row.Note, row.NoteName, row.Velocity, control.Steps.Pattern[index]);
                }

                return;
            }

            rows.Children().Clear();
            m_stepRows.clear();
            m_stepRowsControlId = control.Id;

            for (size_t index = 0; index < count; ++index)
            {
                auto const number = std::to_wstring(index + 1);

                controls::Grid row{};
                row.ColumnSpacing(6.0);

                auto const column = [&row](double width, xaml::GridUnitType unit)
                    {
                        controls::ColumnDefinition definition{};
                        definition.Width(xaml::GridLengthHelper::FromValueAndType(width, unit));
                        row.ColumnDefinitions().Append(definition);
                    };

                column(22.0, xaml::GridUnitType::Pixel);
                column(0.0, xaml::GridUnitType::Auto);
                column(1.0, xaml::GridUnitType::Star);
                column(34.0, xaml::GridUnitType::Pixel);
                column(1.0, xaml::GridUnitType::Star);

                controls::TextBlock label{};
                label.Text(winrt::hstring{ number });
                label.VerticalAlignment(xaml::VerticalAlignment::Center);

                // The row's controls carry the step number in their own names, so reading it
                // again on its own would only be noise.
                xaml::Automation::AutomationProperties::SetAccessibilityView(
                    label, xaml::Automation::Peers::AccessibilityView::Raw);

                controls::CheckBox plays{};
                plays.MinWidth(0.0);
                plays.VerticalAlignment(xaml::VerticalAlignment::Center);
                xaml::Automation::AutomationProperties::SetName(
                    plays, winrt::hstring{ resources::FormatString(L"StepPlaysNameFormat", number) });

                controls::NumberBox note{};
                note.Minimum(0.0);
                note.Maximum(127.0);
                note.SmallChange(1.0);
                note.LargeChange(12.0);
                note.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                xaml::Automation::AutomationProperties::SetName(
                    note, winrt::hstring{ resources::FormatString(L"StepNoteNameFormat", number) });

                controls::TextBlock noteName{};
                noteName.VerticalAlignment(xaml::VerticalAlignment::Center);
                xaml::Automation::AutomationProperties::SetAccessibilityView(
                    noteName, xaml::Automation::Peers::AccessibilityView::Raw);

                controls::NumberBox velocity{};
                velocity.Minimum(LowestVelocityPercent);
                velocity.Maximum(HighestVelocityPercent);
                velocity.SmallChange(1.0);
                velocity.LargeChange(10.0);
                velocity.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                xaml::Automation::AutomationProperties::SetName(
                    velocity, winrt::hstring{ resources::FormatString(L"StepVelocityNameFormat", number) });

                FillStepRow(plays, note, noteName, velocity, control.Steps.Pattern[index]);

                auto const onFlag = [weak = get_weak(), index](foundation::IInspectable const&, xaml::RoutedEventArgs const&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ApplyStepEdit(index, false);
                        }
                    };

                plays.Checked(onFlag);
                plays.Unchecked(onFlag);

                auto const onNumber = [weak = get_weak(), index](controls::NumberBox const&, controls::NumberBoxValueChangedEventArgs const&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ApplyStepEdit(index, true);
                        }
                    };

                note.ValueChanged(onNumber);
                velocity.ValueChanged(onNumber);

                controls::Grid::SetColumn(plays, 1);
                controls::Grid::SetColumn(note, 2);
                controls::Grid::SetColumn(noteName, 3);
                controls::Grid::SetColumn(velocity, 4);

                row.Children().Append(label);
                row.Children().Append(plays);
                row.Children().Append(note);
                row.Children().Append(noteName);
                row.Children().Append(velocity);

                rows.Children().Append(row);

                m_stepRows.push_back({ plays, note, noteName, velocity });
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the steps.")
    }

    void EditorWindow::ApplyStepsSettingsEdit()
    {
        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto settings = control->Steps;

        // Left alone when the combo is blank, which is what a rate that is not on the list looks
        // like. Snapping it would change the layout just by looking at it.
        auto const rateIndex = StepsRateCombo().SelectedIndex();

        if (rateIndex >= 0 && rateIndex < static_cast<int32_t>(std::size(glass::StepRateChoices)))
        {
            settings.StepsPerBeat = glass::StepRateChoices[rateIndex];
        }

        auto const directionIndex = StepsDirectionCombo().SelectedIndex();

        if (directionIndex >= 0 && directionIndex < static_cast<int32_t>(std::size(glass::StepDirectionOrder)))
        {
            settings.Direction = glass::StepDirectionOrder[directionIndex];
        }

        settings.Gate = StepsGateSlider().Value() / 100.0;
        settings.Swing = StepsSwingSlider().Value() / 100.0;
        settings.Latching = StepsLatchingCheck().IsChecked().GetBoolean();
        settings.StartsRunning = StepsStartsRunningCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetStepsSettings(id, settings); });
    }

    _Use_decl_annotations_
    void EditorWindow::ApplyStepEdit(size_t index, bool coalesce)
    {
        if (m_updatingInspector || index >= m_stepRows.size())
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr ||
            control->Id != m_stepRowsControlId ||
            index >= control->Steps.Pattern.size())
        {
            return;
        }

        auto const& row = m_stepRows[index];
        auto step = control->Steps.Pattern[index];

        step.On = row.Plays.IsChecked().GetBoolean();

        // A box emptied by the keyboard reads as not a number. It keeps what it had.
        if (!std::isnan(row.Note.Value()))
        {
            step.Note = std::clamp(static_cast<int32_t>(std::lround(row.Note.Value())), 0, 127);
        }

        if (!std::isnan(row.Velocity.Value()))
        {
            step.Velocity = std::clamp(row.Velocity.Value(), LowestVelocityPercent, HighestVelocityPercent) / 100.0;
        }

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetStep(id, static_cast<int32_t>(index), step, coalesce); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnStepsChoiceChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyStepsSettingsEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnStepsSliderChanged(
        foundation::IInspectable const& sender,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyStepsSettingsEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnStepsFlagChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyStepsSettingsEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnStepCountChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector || std::isnan(sender.Value()))
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto const count = static_cast<int32_t>(std::lround(sender.Value()));

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetStepCount(id, count); });
    }
}
