// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pad grids in rows or columns, and the controls that change them while the layout runs: the
// arrangement in the pads section, the targets of a picker, a pad grid row and a readout, what a
// readout shows, and the button that adds a row of grid buttons under a grid.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "PadGrid.h"
#include "PadGridChanges.h"
#include "Fretboard.h"
#include "EditorPadGridKeys.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        void Show(_In_ xaml::UIElement const& element, _In_ bool visible)
        {
            element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }

        constexpr wchar_t const* ArrangementKeys[]
        {
            L"PadArrangementFlow",
            L"PadArrangementRows",
            L"PadArrangementColumns",
        };

        constexpr wchar_t const* ReadoutSourceKeys[]
        {
            L"ReadoutSourceValue",
            L"ReadoutSourcePadKey",
            L"ReadoutSourceOctave",
            L"ReadoutSourceRowOffset",
            L"ReadoutSourcePadSummary",
            L"ReadoutSourceCapo",
            L"ReadoutSourceTuning",
        };

        bool IsChecked(_In_ controls::CheckBox const& check)
        {
            return check.IsChecked() != nullptr && check.IsChecked().Value();
        }
    }

    void EditorWindow::BuildPadGridChoices()
    {
        for (auto const* const key : ArrangementKeys)
        {
            PadArrangementCombo().Items().Append(box_value(resources::GetString(key)));
        }

        for (auto const* const key : PadGridFunctionKeys)
        {
            PadGridFunctionCombo().Items().Append(box_value(resources::GetString(key)));
        }

        for (auto const* const key : ReadoutSourceKeys)
        {
            ReadoutSourceCombo().Items().Append(box_value(resources::GetString(key)));
        }
    }

    // ---------------------------------------------------------------- rows and columns

    _Use_decl_annotations_
    void EditorWindow::RefreshPadArrangement(glass::Control const& control)
    {
        auto const& spec = control.Pads;
        auto const fixed = spec.Arrangement != glass::PadArrangement::Flow;
        auto const columns = spec.Arrangement == glass::PadArrangement::Columns;

        PadArrangementCombo().SelectedIndex(static_cast<int32_t>(spec.Arrangement));

        Show(PadFlowGrid(), !fixed);
        Show(PadLinesGrid(), fixed);

        PadLinesBox().Header(box_value(resources::GetString(columns ? L"PadColumnsHeader" : L"PadRowsHeader")));
        PadsPerLineBox().Header(box_value(resources::GetString(columns ? L"PadsPerColumnHeader" : L"PadsPerRowHeader")));

        auto const perLine = std::clamp(spec.PadsPerRow, glass::MinimumPadsPerRow, glass::MaximumPadsPerRow);

        PadLinesBox().Value(glass::RowsAllowed(spec.Rows, perLine));
        PadsPerLineBox().Value(perLine);

        // The most lines that fit under the cap, with this many pads in each.
        PadLinesBox().Maximum(glass::RowsAllowed(glass::MaximumGridRows, perLine));

        PadHighlightHeldCheck().IsChecked(spec.HighlightHeld);

        if (fixed)
        {
            PadFlowCaption().Text(resources::FormatString(
                columns ? L"PadColumnsCaptionFormat" : L"PadRowsCaptionFormat",
                std::to_wstring(glass::PadCountOf(spec))));
        }
    }

    _Use_decl_annotations_
    void EditorWindow::ReadPadArrangement(glass::PadGridSpec& pads)
    {
        auto const index = PadArrangementCombo().SelectedIndex();

        if (index >= 0 && index < static_cast<int32_t>(std::size(ArrangementKeys)))
        {
            auto const wanted = static_cast<glass::PadArrangement>(index);

            // Rows and columns are where one note shows up in more than one place, so lighting
            // all of them starts on. Flow keeps whatever it had.
            if (wanted != pads.Arrangement && pads.Arrangement == glass::PadArrangement::Flow)
            {
                pads.HighlightHeld = true;
            }
            else
            {
                pads.HighlightHeld = IsChecked(PadHighlightHeldCheck());
            }

            pads.Arrangement = wanted;
        }
        else
        {
            pads.HighlightHeld = IsChecked(PadHighlightHeldCheck());
        }

        if (!std::isnan(PadLinesBox().Value()))
        {
            pads.Rows = static_cast<int32_t>(std::lround(PadLinesBox().Value()));
        }

        if (!std::isnan(PadsPerLineBox().Value()))
        {
            pads.PadsPerRow = static_cast<int32_t>(std::lround(PadsPerLineBox().Value()));
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnPadArrangementChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyPadGridEdit();
    }

    // ---------------------------------------------------------------- targets

    _Use_decl_annotations_
    void EditorWindow::BuildTargetChecks(
        controls::StackPanel const& panel,
        glass::PadGridTargets const& targets,
        std::function<void(glass::PadGridTargets const&)> apply)
    {
        panel.Children().Clear();

        std::vector<std::wstring> ids{};
        std::vector<controls::CheckBox> grids{};

        controls::CheckBox all{};
        all.Content(box_value(resources::GetString(L"TargetsAllGrids")));
        all.IsChecked(targets.AllGrids);
        panel.Children().Append(all);

        int32_t unnamed{ 0 };

        for (auto const& page : m_editor.Document().Pages)
        {
            for (auto const& control : page.Controls)
            {
                if (!(glass::IsPadGrid(control.Kind) || glass::IsFretboard(control.Kind)) || control.Id.empty())
                {
                    continue;
                }

                controls::CheckBox check{};

                check.Content(box_value(control.Label.empty()
                    ? resources::FormatString(L"TargetsUnnamedGridFormat", ++unnamed)
                    : winrt::hstring{ control.Label }));

                check.IsChecked(targets.AllGrids ||
                    std::find(targets.ControlIds.begin(), targets.ControlIds.end(), control.Id) != targets.ControlIds.end());
                check.IsEnabled(!targets.AllGrids);
                check.Margin(xaml::ThicknessHelper::FromLengths(16, 0, 0, 0));

                panel.Children().Append(check);

                ids.push_back(control.Id);
                grids.push_back(check);
            }
        }

        if (!targets.AllGrids && targets.ControlIds.empty())
        {
            controls::TextBlock none{};

            none.Text(resources::GetString(L"TargetsNoGrids"));
            none.FontSize(11);
            none.Margin(xaml::ThicknessHelper::FromLengths(16, 2, 0, 0));

            panel.Children().Append(none);
        }

        // The handlers go on after the boxes are filled in, so filling them in edits nothing. They
        // hold the boxes weakly: a box holding a handler that holds the box would never be freed.
        std::vector<winrt::weak_ref<controls::CheckBox>> weakGrids{};

        for (auto const& check : grids)
        {
            weakGrids.push_back(winrt::make_weak(check));
        }

        auto const changed = [weak = get_weak(), weakAll = winrt::make_weak(all), weakGrids, ids, apply](
            foundation::IInspectable const&, xaml::RoutedEventArgs const&)
            {
                auto strong = weak.get();
                auto const allBox = weakAll.get();

                if (strong == nullptr || allBox == nullptr || strong->m_updatingInspector)
                {
                    return;
                }

                try
                {
                    glass::PadGridTargets wanted{};

                    wanted.AllGrids = IsChecked(allBox);

                    if (!wanted.AllGrids)
                    {
                        for (size_t index = 0; index < weakGrids.size() && index < ids.size(); ++index)
                        {
                            auto const box = weakGrids[index].get();

                            if (box != nullptr && IsChecked(box))
                            {
                                wanted.ControlIds.push_back(ids[index]);
                            }
                        }
                    }

                    apply(wanted);
                }
                MIDI_GLASS_CATCH_AND_LOG(L"Unable to change which pad grids this changes.")
            };

        all.Checked(changed);
        all.Unchecked(changed);

        for (auto const& check : grids)
        {
            check.Checked(changed);
            check.Unchecked(changed);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshSwitchTargets(glass::Control const& control)
    {
        auto const picking = glass::IsKeyPicker(control);

        Show(SwitchTargetsPanel(), picking);

        if (!picking)
        {
            SwitchTargetChecks().Children().Clear();
            return;
        }

        auto const id = control.Id;

        BuildTargetChecks(SwitchTargetChecks(), control.Switch.Targets,
            [weak = get_weak(), id](glass::PadGridTargets const& targets)
            {
                if (auto strong = weak.get())
                {
                    strong->ApplyControlEdit(id, [&](std::wstring const& controlId)
                        { return strong->m_editor.SetSwitchTargets(controlId, targets); });
                }
            });
    }

    // ---------------------------------------------------------------- a readout

    _Use_decl_annotations_
    void EditorWindow::RefreshReadoutPanel(glass::Control const& control)
    {
        auto const& readout = control.Readout;
        auto const showsGrid = readout.Source != glass::ReadoutSource::Value;

        ReadoutSourceCombo().SelectedIndex(static_cast<int32_t>(readout.Source));
        Show(ReadoutTargetsPanel(), showsGrid);

        if (!showsGrid)
        {
            ReadoutTargetChecks().Children().Clear();
            return;
        }

        auto const id = control.Id;

        BuildTargetChecks(ReadoutTargetChecks(), readout.Targets,
            [weak = get_weak(), id](glass::PadGridTargets const& targets)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                auto const* const current = strong->m_editor.Document().FindControl(id);

                if (current == nullptr)
                {
                    return;
                }

                auto wanted = current->Readout;
                wanted.Targets = targets;

                strong->ApplyControlEdit(id, [&](std::wstring const& controlId)
                    { return strong->m_editor.SetReadout(controlId, wanted); });
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnReadoutSourceChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();
        auto const index = ReadoutSourceCombo().SelectedIndex();

        if (control == nullptr || control->Kind != glass::ControlKind::Readout ||
            index < 0 || index >= static_cast<int32_t>(std::size(ReadoutSourceKeys)))
        {
            return;
        }

        try
        {
            auto const id = control->Id;
            auto wanted = control->Readout;

            wanted.Source = static_cast<glass::ReadoutSource>(index);

            ApplyControlEdit(id, [&](std::wstring const& controlId)
                { return m_editor.SetReadout(controlId, wanted); });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change what this readout shows.")
    }

    // ---------------------------------------------------------------- a pad grid row

    _Use_decl_annotations_
    void EditorWindow::RefreshPadGridRowFields(glass::ControlMessage const& message)
    {
        PadGridFunctionCombo().SelectedIndex(static_cast<int32_t>(message.PadFunction));

        BuildTargetChecks(RowTargetChecks(), message.PadTargets,
            [weak = get_weak()](glass::PadGridTargets const& targets)
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                strong->TryEditSelectedMessage([&targets](glass::ControlMessage& message)
                    {
                        if (message.PadTargets == targets)
                        {
                            return false;
                        }

                        message.PadTargets = targets;
                        return true;
                    });

                // All grids turns the other boxes on and off, so they're built again.
                glass::ControlMessage shown{};
                shown.PadFunction = static_cast<glass::PadGridFunction>(
                    std::clamp(strong->PadGridFunctionCombo().SelectedIndex(), 0, glass::PadGridFunctionCount - 1));
                shown.PadTargets = targets;

                strong->RefreshPadGridRowFields(shown);
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPadGridFunctionChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        try
        {
            auto const index = PadGridFunctionCombo().SelectedIndex();

            if (index < 0 || index >= glass::PadGridFunctionCount)
            {
                return;
            }

            auto const function = static_cast<glass::PadGridFunction>(index);

            TryEditSelectedMessage([function](glass::ControlMessage& message)
                {
                    if (message.PadFunction == function)
                    {
                        return false;
                    }

                    message.PadFunction = function;
                    return true;
                });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change what this row does.")
    }

    // ---------------------------------------------------------------- grid buttons

    _Use_decl_annotations_
    void EditorWindow::OnAddGridButtonsClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        PadGridButtonsFlyout().Hide();

        auto const* const control = SingleSelectedControl();

        if (control == nullptr || !glass::IsPadGrid(control->Kind))
        {
            return;
        }

        try
        {
            // Copied, because adding to the page can move the grid in memory.
            auto const gridId = control->Id;

            glass::EditorController::GridButtonChoices choices{};

            choices.Octave = IsChecked(GridButtonsOctaveCheck());
            choices.Rows = IsChecked(GridButtonsRowsCheck());
            choices.InKeyOnly = IsChecked(GridButtonsInKeyCheck());
            choices.Readout = IsChecked(GridButtonsReadoutCheck());
            choices.Reset = IsChecked(GridButtonsResetCheck());

            glass::EditorController::GridButtonLabels labels{};

            labels.OctaveDown = resources::GetString(L"GridButtonOctaveDownLabel");
            labels.OctaveUp = resources::GetString(L"GridButtonOctaveUpLabel");
            labels.RowDown = resources::GetString(L"GridButtonRowDownLabel");
            labels.RowUp = resources::GetString(L"GridButtonRowUpLabel");
            labels.InKeyOnly = resources::GetString(L"GridButtonInKeyOnlyLabel");
            labels.Readout = resources::GetString(L"GridButtonReadoutLabel");
            labels.Reset = resources::GetString(L"GridButtonResetLabel");

            if (m_editor.AddGridButtons(gridId, choices, labels).empty())
            {
                return;
            }

            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            UpdateStatusBar();
            MarkChanged();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add the grid buttons.")
    }
}
