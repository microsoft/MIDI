// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// What the per-kind inspector panels do when somebody changes one of them, and the picture
// picker that copies a chosen file next to the layout.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "LayoutStore.h"

#include <shobjidl.h>
#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr glass::FeedbackMode FeedbackModeOrder[]
        {
            glass::FeedbackMode::Message,
            glass::FeedbackMode::AnyActivity,
            glass::FeedbackMode::Tempo,
        };

        constexpr glass::BackgroundFit PictureFitOrder[]
        {
            glass::BackgroundFit::Uniform,
            glass::BackgroundFit::Stretch,
            glass::BackgroundFit::Centered,
            glass::BackgroundFit::Tiled,
        };

        constexpr glass::DragAxis DragAxisOrder[]
        {
            glass::DragAxis::Vertical,
            glass::DragAxis::Horizontal,
        };

        constexpr glass::MessageKind FeedbackKindOrder[]
        {
            glass::MessageKind::ControlChange,
            glass::MessageKind::Note,
            glass::MessageKind::PitchBend,
            glass::MessageKind::ChannelPressure,
        };

        constexpr double SpringTargetValues[]{ 0.0, 0.5, 1.0 };
    }

    // One path for every per-kind edit, so none of them can forget to redraw the surface, put
    // the panel back in step with what was actually written, or mark the layout changed.
    _Use_decl_annotations_
    void EditorWindow::ApplyControlEdit(
        std::wstring const& id,
        std::function<bool(std::wstring const&)> const& edit)
    {
        try
        {
            if (!edit(id))
            {
                return;
            }

            RebuildSurface();
            MarkChanged();

            if (auto const* const refreshed = m_editor.Document().FindControl(id))
            {
                auto const previous = m_updatingInspector;
                m_updatingInspector = true;

                RefreshKindPanels(*refreshed);

                m_updatingInspector = previous;
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to change this control's properties.")
    }

    // ---------------------------------------------------------------- marks and stops

    _Use_decl_annotations_
    void EditorWindow::OnTicksShowChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto ticks = control->Ticks;
        ticks.Show = TicksShowCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlTicks(id, ticks); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnTickCountChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr || std::isnan(TickCountBox().Value()))
        {
            return;
        }

        auto ticks = control->Ticks;
        ticks.Count = static_cast<int32_t>(std::lround(TickCountBox().Value()));

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlTicks(id, ticks); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnShowDetentValuesChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto const show = ShowDetentValuesCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlShowDetentValues(id, show); });
    }

    // ---------------------------------------------------------------- the picture

    // The Win32 common item dialog, not Windows.Storage.Pickers: this is a desktop app and the
    // picker needs a window handle it can be modal to.
    std::wstring EditorWindow::PickControlPictureFile()
    {
        try
        {
            auto dialog = winrt::create_instance<IFileOpenDialog>(CLSID_FileOpenDialog);

            if (dialog == nullptr)
            {
                return {};
            }

            COMDLG_FILTERSPEC const filters[]
            {
                { L"Pictures and video (*.png;*.jpg;*.jpeg;*.svg;*.mp4;*.m4v;*.mkv;*.webm;*.wmv)",
                  L"*.png;*.jpg;*.jpeg;*.svg;*.mp4;*.m4v;*.mkv;*.webm;*.wmv" },
                { L"Pictures (*.png;*.jpg;*.jpeg;*.svg)", L"*.png;*.jpg;*.jpeg;*.svg" },
            };

            dialog->SetFileTypes(static_cast<UINT>(std::size(filters)), filters);
            dialog->SetTitle(resources::GetString(L"PictureOpenTitle").c_str());

            if (FAILED(dialog->Show(m_chrome.WindowHandle())))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to choose a picture.")

        return {};
    }

    _Use_decl_annotations_
    void EditorWindow::OnChoosePictureClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                return;
            }

            auto const id = control->Id;
            auto picture = control->Image;

            auto const chosen = PickControlPictureFile();

            if (chosen.empty())
            {
                return;
            }

            if (!glass::IsSupportedPictureFileName(chosen))
            {
                return;
            }

            // The picture lives beside the layout so the two travel together. A layout that has
            // never been saved has no folder to copy into, so it is saved first.
            if (m_editor.Document().FilePath.empty())
            {
                SaveNow();
            }

            auto const copied = glass::CopyBackgroundImageBeside(
                chosen, m_editor.Document().FilePath);

            if (copied.empty())
            {
                return;
            }

            picture.FileName = copied;

            ApplyControlEdit(id, [&](std::wstring const& controlId)
                { return m_editor.SetControlPicture(controlId, picture); });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set this control's picture.")
    }

    _Use_decl_annotations_
    void EditorWindow::OnRemovePictureClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        // Only the reference is dropped. The file stays where it was copied to, because another
        // control on the layout may be showing it and a delete cannot be undone.
        glass::Picture picture{};

        picture.Fit = control->Image.Fit;
        picture.Opacity = control->Image.Opacity;
        picture.Loops = control->Image.Loops;

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureFitChanged(
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
        auto const index = PictureFitCombo().SelectedIndex();

        if (control == nullptr || index < 0 || index >= static_cast<int32_t>(std::size(PictureFitOrder)))
        {
            return;
        }

        auto picture = control->Image;
        picture.Fit = PictureFitOrder[index];

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureOpacityChanged(
        foundation::IInspectable const& sender,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto picture = control->Image;
        picture.Opacity = std::clamp(args.NewValue() / 100.0, 0.0, 1.0);

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureLoopsChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const* const control = SingleSelectedControl();

        if (control == nullptr)
        {
            return;
        }

        auto picture = control->Image;
        picture.Loops = PictureLoopsCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    // ---------------------------------------------------------------- the keyboard

    void EditorWindow::ApplyKeyboardEdit()
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

        auto keyboard = control->Keyboard;

        if (!std::isnan(KeyCountBox().Value()))
        {
            keyboard.KeyCount = static_cast<int32_t>(std::lround(KeyCountBox().Value()));
        }

        if (!std::isnan(LowestNoteBox().Value()))
        {
            keyboard.LowestNote = static_cast<int32_t>(std::lround(LowestNoteBox().Value()));
        }

        keyboard.WhiteKeyColor = std::wstring{ WhiteKeyColorBox().Text() };
        keyboard.BlackKeyColor = std::wstring{ BlackKeyColorBox().Text() };
        keyboard.PressedKeyColor = std::wstring{ PressedKeyColorBox().Text() };
        keyboard.VelocityFromKeyPosition = KeyVelocityCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlKeyboard(id, keyboard); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyCountChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyKeyboardEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnLowestNoteChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyKeyboardEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyColorChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyKeyboardEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnKeyVelocityChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyKeyboardEdit();
    }

    // ---------------------------------------------------------------- how it behaves

    _Use_decl_annotations_
    void EditorWindow::OnSpringTargetChanged(
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
        auto const index = SpringTargetCombo().SelectedIndex();

        // The last entry is "wherever the slider is", which is a description of what is already
        // set rather than a value to write.
        if (control == nullptr || index < 0 ||
            index >= static_cast<int32_t>(std::size(SpringTargetValues)))
        {
            return;
        }

        auto const value = SpringTargetValues[index];

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlDefaultValue(id, value); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnDragAxisChanged(
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
        auto const index = DragAxisCombo().SelectedIndex();

        if (control == nullptr || index < 0 || index >= static_cast<int32_t>(std::size(DragAxisOrder)))
        {
            return;
        }

        auto const drag = DragAxisOrder[index];

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlDrag(id, drag); });
    }

    // ---------------------------------------------------------------- the clock

    void EditorWindow::ApplyClockEdit()
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

        auto clock = control->Clock;

        if (!std::isnan(ClockBpmBox().Value()))
        {
            clock.BeatsPerMinute = ClockBpmBox().Value();
        }

        if (!std::isnan(ClockLowestBpmBox().Value()))
        {
            clock.LowestBeatsPerMinute = ClockLowestBpmBox().Value();
        }

        if (!std::isnan(ClockHighestBpmBox().Value()))
        {
            clock.HighestBeatsPerMinute = ClockHighestBpmBox().Value();
        }

        clock.StartsRunning = ClockStartsRunningCheck().IsChecked().GetBoolean();
        clock.SendsTransport = ClockSendsTransportCheck().IsChecked().GetBoolean();

        auto const index = ClockSourceCombo().SelectedIndex();

        if (index >= 0 && static_cast<size_t>(index) < m_tempoSourceIds.size())
        {
            clock.TempoControlId = m_tempoSourceIds[static_cast<size_t>(index)];
        }

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlClock(id, clock); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnClockBpmChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyClockEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnClockRangeChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyClockEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnClockSourceChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyClockEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnClockFlagChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyClockEdit();
    }

    // ---------------------------------------------------------------- the stops

    _Use_decl_annotations_
    void EditorWindow::OnDetentModeChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const index = DetentModeCombo().SelectedIndex();

        if (index < 0 || index > 2)
        {
            return;
        }

        auto const mode = static_cast<glass::DetentMode>(index);

        if (TryEditSelectedMessage([mode](glass::ControlMessage& message)
            {
                if (message.Detents.Mode == mode)
                {
                    return false;
                }

                message.Detents.Mode = mode;

                // A list with nothing in it and a step of zero both mean "smooth", which is not
                // what somebody who just picked one of these meant. Both get a usable start.
                if (mode == glass::DetentMode::EvenSteps && message.Detents.Step <= 0.0)
                {
                    message.Detents.Step = 0.1;
                }

                if (mode == glass::DetentMode::ExplicitValues && message.Detents.Stops.empty())
                {
                    message.Detents.Stops = { 0.0, 0.5, 1.0 };
                }

                return true;
            }))
        {
            RebuildSurface();
            RefreshMessageFields();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnDetentStepChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector || std::isnan(DetentStepBox().Value()))
        {
            return;
        }

        auto const step = DetentStepBox().Value();

        if (TryEditSelectedMessage([step](glass::ControlMessage& message)
            {
                if (message.Detents.Step == step)
                {
                    return false;
                }

                message.Detents.Step = step;

                return true;
            }))
        {
            RebuildSurface();
            RefreshMessageFields();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnDetentStopsChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (m_updatingInspector)
        {
            return;
        }

        auto const stops = glass::ParseStopList(std::wstring{ DetentStopsBox().Text() });

        if (TryEditSelectedMessage([&stops](glass::ControlMessage& message)
            {
                if (message.Detents.Stops == stops)
                {
                    return false;
                }

                message.Detents.Stops = stops;

                return true;
            }))
        {
            RebuildSurface();
            RefreshMessageFields();
        }
    }

    // ---------------------------------------------------------------- what it listens for

    void EditorWindow::ApplyFeedbackEdit()
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

        auto feedback = control->Feedback;

        feedback.Enabled = FeedbackEnabledSwitch().IsOn();

        auto const modeIndex = FeedbackModeCombo().SelectedIndex();

        if (modeIndex >= 0 && modeIndex < static_cast<int32_t>(std::size(FeedbackModeOrder)))
        {
            feedback.Mode = FeedbackModeOrder[modeIndex];
        }

        auto const kindIndex = FeedbackKindCombo().SelectedIndex();

        if (kindIndex >= 0 && kindIndex < static_cast<int32_t>(std::size(FeedbackKindOrder)))
        {
            feedback.Kind = FeedbackKindOrder[kindIndex];
        }

        auto const deviceIndex = FeedbackDeviceCombo().SelectedIndex();

        if (deviceIndex >= 0 && static_cast<size_t>(deviceIndex) < m_feedbackDeviceNames.size())
        {
            feedback.DeviceName = m_feedbackDeviceNames[static_cast<size_t>(deviceIndex)];
        }

        // The first entry is "any group", which the model writes as -1.
        auto const groupIndex = FeedbackGroupCombo().SelectedIndex();

        feedback.GroupIndex = groupIndex <= 0 ? glass::AllGroups : groupIndex - 1;

        feedback.ChannelIndex = std::max(0, FeedbackChannelCombo().SelectedIndex());
        feedback.MatchesChannel = FeedbackMatchChannelCheck().IsChecked().GetBoolean();

        if (!std::isnan(FeedbackNumberBox().Value()))
        {
            feedback.Number = static_cast<uint32_t>(
                std::max<int64_t>(0, std::llround(FeedbackNumberBox().Value())));
        }

        if (!std::isnan(FeedbackHoldBox().Value()))
        {
            feedback.HoldMilliseconds = static_cast<int32_t>(std::lround(FeedbackHoldBox().Value()));
        }

        auto const tempoIndex = FeedbackTempoCombo().SelectedIndex();

        if (tempoIndex >= 0 && static_cast<size_t>(tempoIndex) < m_beatSourceIds.size())
        {
            feedback.TempoControlId = m_beatSourceIds[static_cast<size_t>(tempoIndex)];
        }

        auto const id = control->Id;

        if (m_editor.SetFeedback(id, feedback))
        {
            RebuildSurface();
            MarkChanged();
        }

        if (auto const* const refreshed = m_editor.Document().FindControl(id))
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            RefreshFeedbackPanel(*refreshed);

            m_updatingInspector = previous;
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackEnabledToggled(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackModeChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackDeviceChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackGroupChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackChannelChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackKindChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackTempoChanged(
        foundation::IInspectable const& sender,
        controls::SelectionChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackMatchChannelChanged(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackNumberChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }

    _Use_decl_annotations_
    void EditorWindow::OnFeedbackHoldChanged(
        controls::NumberBox const& sender,
        controls::NumberBoxValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        ApplyFeedbackEdit();
    }
}
