// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Root and scale pickers in the editor: what a switch is used for, the scales a scale picker
// offers, the button on a pad grid that adds both, and a pick made in Try mode.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "GlassControl.h"
#include "PadGrid.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        void Show(_In_ xaml::UIElement const& element, _In_ bool visible)
        {
            element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
        }
    }

    void EditorWindow::BuildKeyPickerChoices()
    {
        // In the order of SwitchPicks.
        for (auto const* const key : { L"SwitchPicksNothing", L"SwitchPicksRootNote", L"SwitchPicksScale" })
        {
            SwitchPicksCombo().Items().Append(box_value(resources::GetString(key)));
        }

        // One box per scale, in the order of PadScaleOrder, which is how they are read back.
        for (auto const scale : glass::PadScaleOrder)
        {
            controls::CheckBox check{};

            check.Content(box_value(resources::GetString(glass::ScaleNameResourceKey(scale))));

            auto const changed = [weak = get_weak()](foundation::IInspectable const&, xaml::RoutedEventArgs const&)
                {
                    if (auto strong = weak.get())
                    {
                        strong->ApplyPickerScales();
                    }
                };

            check.Checked(changed);
            check.Unchecked(changed);

            SwitchScaleChecks().Children().Append(check);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshKeyPickerPanel(glass::Control const& control)
    {
        auto const picks = control.Switch.Picks;
        auto const picking = glass::IsKeyPicker(control);

        SwitchPicksCombo().SelectedIndex(static_cast<int32_t>(picks));

        // A picker's positions are the notes or the scales, so there is nothing to name or add.
        Show(SwitchPositionRows(), !picking);
        Show(AddSwitchPositionButton(), !picking);
        Show(SwitchPositionsCaption(), !picking);
        Show(SwitchPicksCaption(), picking);
        Show(SwitchScalesPanel(), picks == glass::SwitchPicks::Scale);

        if (picking)
        {
            SwitchPicksCaption().Text(resources::GetString(
                picks == glass::SwitchPicks::RootNote ? L"SwitchPicksRootCaption" : L"SwitchPicksScaleCaption"));
        }

        if (picks != glass::SwitchPicks::Scale)
        {
            return;
        }

        auto const& scales = control.Switch.Scales;
        auto const children = SwitchScaleChecks().Children();

        for (uint32_t index = 0; index < children.Size() && index < std::size(glass::PadScaleOrder); ++index)
        {
            auto const check = children.GetAt(index).try_as<controls::CheckBox>();

            if (check == nullptr)
            {
                continue;
            }

            auto const offered = std::find(scales.begin(), scales.end(), glass::PadScaleOrder[index]) != scales.end();

            check.IsChecked(offered);

            // A picker with one scale has nothing to pick, so the last two stay.
            check.IsEnabled(!offered || scales.size() > 2);
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshPadGridPickers(glass::Control const& control)
    {
        auto const& document = m_editor.Document();
        auto const keyed = control.Pads.KeyRoot != glass::NoKey;
        auto const both = glass::HasKeyPicker(document, glass::SwitchPicks::RootNote) &&
            glass::HasKeyPicker(document, glass::SwitchPicks::Scale);

        PadAddPickersButton().IsEnabled(keyed && !both);

        PadPickersCaption().Text(resources::GetString(
            !keyed ? L"PadPickersNeedKeyCaption" : (both ? L"PadPickersOnLayoutCaption" : L"PadPickersCaption")));
    }

    _Use_decl_annotations_
    void EditorWindow::OnSwitchPicksChanged(
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
        auto const index = SwitchPicksCombo().SelectedIndex();

        if (control == nullptr || index < 0 || index > static_cast<int32_t>(glass::SwitchPicks::Scale))
        {
            return;
        }

        auto const picks = static_cast<glass::SwitchPicks>(index);

        if (picks == control->Switch.Picks)
        {
            return;
        }

        try
        {
            auto const id = control->Id;

            if (!m_editor.SetSwitchPicks(id, picks))
            {
                return;
            }

            // The switch can grow, and a picker has no Sends tab, so the whole inspector catches up.
            RebuildSurface();
            RefreshInspector();
            MarkChanged();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change what this switch picks.")
    }

    void EditorWindow::ApplyPickerScales()
    {
        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr || control->Switch.Picks != glass::SwitchPicks::Scale)
        {
            return;
        }

        try
        {
            std::vector<glass::MusicalScale> scales{};
            auto const children = SwitchScaleChecks().Children();

            for (uint32_t index = 0; index < children.Size() && index < std::size(glass::PadScaleOrder); ++index)
            {
                auto const check = children.GetAt(index).try_as<controls::CheckBox>();

                if (check != nullptr && check.IsChecked() != nullptr && check.IsChecked().Value())
                {
                    scales.push_back(glass::PadScaleOrder[index]);
                }
            }

            auto const id = control->Id;

            ApplyControlEdit(id, [&](std::wstring const& controlId)
                { return m_editor.SetSwitchScales(controlId, scales); });

            // The preview has a slice for each scale, so it is drawn again with them.
            if (auto const* const refreshed = m_editor.Document().FindControl(id))
            {
                RefreshPreview(*refreshed);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change the scales this switch offers.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnAddKeyPickersClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control == nullptr || !glass::IsPadGrid(control->Kind))
        {
            return;
        }

        try
        {
            // Copied, because adding to the page can move the grid in memory.
            auto const gridId = control->Id;

            auto const added = m_editor.AddKeyPickers(
                gridId,
                std::wstring{ resources::GetString(L"RootPickerLabel") },
                std::wstring{ resources::GetString(L"ScalePickerLabel") });

            if (added.empty())
            {
                return;
            }

            RebuildSurface();
            RebuildOutline();
            RefreshInspector();
            UpdateStatusBar();
            MarkChanged();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to add the root and scale pickers.")
    }

    // ---------------------------------------------------------------- Try mode

    _Use_decl_annotations_
    void EditorWindow::OnTryKeyPicked(glass::PickedKey const& picked)
    {
        if (!m_tryMode)
        {
            return;
        }

        try
        {
            for (auto const& [itemIndex, value] : m_renderer.ApplyPickedKey(picked, m_editor.Document(), m_theme))
            {
                if (auto element = m_renderer.ElementAt(itemIndex))
                {
                    winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(value);
                }
            }

            m_input.RefreshPadGrids();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the key that was picked.")
    }

    // Edit mode shows the layout as it is saved, which is the key it opens in.
    void EditorWindow::ForgetTryKey()
    {
        try
        {
            if (m_player != nullptr)
            {
                m_player->ForgetPickedKey();
            }

            auto const picked = m_renderer.PickedKeyNow();

            if (picked.Root < 0 && picked.Scale < 0)
            {
                return;
            }

            for (auto const& [itemIndex, value] : m_renderer.ApplyPickedKey({}, m_editor.Document(), m_theme))
            {
                if (auto element = m_renderer.ElementAt(itemIndex))
                {
                    winrt::get_self<implementation::GlassControl>(element)->SetValueDirect(value);
                }
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to put the pads back in their saved key.")
    }
}
