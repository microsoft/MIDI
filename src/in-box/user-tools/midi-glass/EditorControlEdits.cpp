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
#include "LayoutPackage.h"

#include <shobjidl.h>
#include <filesystem>

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        constexpr glass::FeedbackMode FeedbackModeOrder[]
        {
            glass::FeedbackMode::AnyActivity,
            glass::FeedbackMode::Notes,
            glass::FeedbackMode::ControlChanges,
            glass::FeedbackMode::Message,
            glass::FeedbackMode::Tempo,
            glass::FeedbackMode::Transport,
        };

        constexpr glass::BackgroundFit PictureFitOrder[]
        {
            glass::BackgroundFit::Uniform,
            glass::BackgroundFit::Fill,
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

    _Use_decl_annotations_
    void EditorWindow::OnPadVelocityChanged(
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

        auto const fromTouch = PadVelocityCheck().IsChecked().GetBoolean();

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlVelocityFromTouch(id, fromTouch); });
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

            // Kept in step with IsSupportedPictureFileName. Anything not on this list is
            // refused when it is copied beside the layout, so offering it here only wastes
            // somebody's time.
            constexpr wchar_t PictureExtensions[] = L"*.png;*.jpg;*.jpeg;*.svg";
            constexpr wchar_t VideoExtensions[] =
                L"*.mp4;*.m4v;*.mkv;*.webm;*.wmv;*.avi;*.mov;*.mpeg;*.mpg;*.m2v;*.asf";

            auto const both = std::wstring{ PictureExtensions } + L";" + VideoExtensions;

            auto const bothLabel = resources::FormatString(L"PictureFilterBothFormat", both);
            auto const pictureLabel =
                resources::FormatString(L"PictureFilterPicturesFormat", PictureExtensions);
            auto const videoLabel =
                resources::FormatString(L"PictureFilterVideoFormat", VideoExtensions);

            COMDLG_FILTERSPEC const filters[]
            {
                { bothLabel.c_str(), both.c_str() },
                { pictureLabel.c_str(), PictureExtensions },
                { videoLabel.c_str(), VideoExtensions },
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

        ChoosePictureAsync();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget EditorWindow::ChoosePictureAsync()
    {
        auto lifetime = get_strong();

        try
        {
            auto const* const control = SingleSelectedControl();

            if (control == nullptr)
            {
                co_return;
            }

            auto const id = control->Id;
            auto picture = control->Image;

            auto const chosen = PickControlPictureFile();

            if (chosen.empty())
            {
                co_return;
            }

            if (!glass::IsSupportedPictureFileName(chosen))
            {
                co_return;
            }

            if (!co_await ConfirmPictureSizeAsync(chosen))
            {
                co_return;
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
                co_return;
            }

            picture.FileName = copied;

            ApplyControlEdit(id, [&](std::wstring const& controlId)
                { return m_editor.SetControlPicture(controlId, picture); });
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to set this control's picture.")
    }

    // A picture is copied next to the layout so the two travel together, which is fine for a
    // photograph and quite a lot to spring on somebody for a two gigabyte video. Say the number
    // before the copy starts, not after.
    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncOperation<bool> EditorWindow::ConfirmPictureSizeAsync(
        std::wstring filePath)
    {
        auto lifetime = get_strong();

        uint64_t bytes{ 0 };

        try
        {
            std::error_code ignored{};

            bytes = static_cast<uint64_t>(std::filesystem::file_size(filePath, ignored));
        }
        catch (...)
        {
        }

        if (bytes <= glass::LargePackageBytes)
        {
            co_return true;
        }

        try
        {
            controls::TextBlock text{};

            text.TextWrapping(xaml::TextWrapping::Wrap);
            text.MaxWidth(420.0);
            text.Text(winrt::hstring{ resources::FormatString(
                L"PictureLargeBodyFormat",
                std::filesystem::path{ filePath }.filename().wstring(),
                resources::DescribeFileSize(bytes)) });

            controls::ContentDialog dialog{};

            dialog.XamlRoot(RootGrid().XamlRoot());
            dialog.Title(box_value(resources::GetString(L"PictureLargeTitle")));
            dialog.Content(text);
            dialog.PrimaryButtonText(resources::GetString(L"PictureLargeAction"));
            dialog.CloseButtonText(resources::GetString(L"DialogCancel"));
            dialog.DefaultButton(controls::ContentDialogButton::Primary);

            m_openDialog = dialog.ShowAsync();
            auto const result = co_await m_openDialog;
            m_openDialog = nullptr;

            co_return result == controls::ContentDialogResult::Primary;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to ask about the size of the picture.")

        co_return false;
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

    // The three crop sliders, which differ only in which field they land in.
    _Use_decl_annotations_
    void EditorWindow::ApplyPictureCropEdit(
        double value,
        void (*assign)(glass::Picture&, double))
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

        auto picture = control->Image;

        assign(picture, value);

        ApplyControlEdit(control->Id, [&](std::wstring const& id)
            { return m_editor.SetControlPicture(id, picture); });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureZoomChanged(
        foundation::IInspectable const& sender,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        ApplyPictureCropEdit(args.NewValue() / 100.0, [](glass::Picture& picture, double value)
            {
                picture.Zoom = value;
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCenterXChanged(
        foundation::IInspectable const& sender,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        ApplyPictureCropEdit(args.NewValue() / 100.0, [](glass::Picture& picture, double value)
            {
                picture.CenterX = value;
            });
    }

    _Use_decl_annotations_
    void EditorWindow::OnPictureCenterYChanged(
        foundation::IInspectable const& sender,
        controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        ApplyPictureCropEdit(args.NewValue() / 100.0, [](glass::Picture& picture, double value)
            {
                picture.CenterY = value;
            });
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
                    // However many stops the control already had, spread evenly between its
                    // two ends. Somebody switching from even steps to a list is refining what
                    // is there, not starting again.
                    auto const lowest = message.Minimum.Value;
                    auto const highest = message.Maximum.Value;

                    auto count = 2;

                    if (message.Detents.Mode == glass::DetentMode::EvenSteps &&
                        message.Detents.Step > 0.0)
                    {
                        auto const span = std::abs(highest - lowest);

                        if (span > 0.0)
                        {
                            count = std::clamp(
                                static_cast<int32_t>(std::floor(span / message.Detents.Step)) + 1,
                                2,
                                static_cast<int32_t>(glass::MaximumDetentStops));
                        }
                    }

                    for (int32_t stop = 0; stop < count; ++stop)
                    {
                        auto const fraction =
                            static_cast<double>(stop) / static_cast<double>(count - 1);

                        message.Detents.Stops.push_back(lowest + (highest - lowest) * fraction);
                    }
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

    // One row per stop: a number box, a caption saying what that stop is, and a way to take it
    // away. The two ends are the range's own ends and cannot be removed, because a range with
    // one end missing is not a range.
    void EditorWindow::RebuildDetentStopRows()
    {
        try
        {
            DetentStopsHost().Items().Clear();

            auto const* const control = SingleSelectedControl();

            if (control == nullptr ||
                m_messageIndex < 0 ||
                m_messageIndex >= static_cast<int32_t>(control->Messages.size()))
            {
                return;
            }

            auto const& message = control->Messages[static_cast<size_t>(m_messageIndex)];
            auto const& stops = message.Detents.Stops;

            auto const absolute =
                message.Detents.Scaling == glass::ValueScaling::Absolute ||
                message.Minimum.Scaling == glass::ValueScaling::Absolute ||
                message.Maximum.Scaling == glass::ValueScaling::Absolute;

            auto weak = get_weak();

            for (size_t index = 0; index < stops.size(); ++index)
            {
                controls::Grid row{};

                row.ColumnSpacing(6.0);

                for (auto const width : {
                    xaml::GridLengthHelper::FromValueAndType(1.0, xaml::GridUnitType::Star),
                    xaml::GridLengthHelper::FromValueAndType(0.0, xaml::GridUnitType::Auto) })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    row.ColumnDefinitions().Append(column);
                }

                controls::NumberBox box{};

                box.Value(stops[index]);
                box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Compact);
                box.SmallChange(absolute ? 1.0 : 0.05);

                // The ends are the range's ends, so they are not free numbers: an absolute
                // range counts in the device's own units and a fraction counts 0 to 1.
                box.Minimum(absolute ? 0.0 : 0.0);
                box.Maximum(absolute ? 4294967295.0 : 1.0);

                auto const header =
                    index == 0 ? L"DetentStopLowest"
                    : index + 1 == stops.size() ? L"DetentStopHighest"
                    : L"DetentStopBetween";

                box.Header(box_value(index == 0 || index + 1 == stops.size()
                    ? resources::GetString(header)
                    : resources::FormatString(header, std::to_wstring(index + 1))));

                xaml::Automation::AutomationProperties::SetName(
                    box, winrt::unbox_value<winrt::hstring>(box.Header()));

                controls::Grid::SetColumn(box, 0);

                box.ValueChanged([weak, index](auto&&, auto&& valueArgs)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr || strong->m_updatingInspector)
                        {
                            return;
                        }

                        if (std::isnan(valueArgs.NewValue()))
                        {
                            return;
                        }

                        strong->SetDetentStop(index, valueArgs.NewValue());
                    });

                row.Children().Append(box);

                // Only the ones between the ends can go. Two stops is the fewest that means
                // anything, so the button is there and disabled rather than missing.
                controls::Button remove{};

                remove.Content(box_value(winrt::hstring{ L"\uE738" }));
                remove.FontFamily(media::FontFamily{ L"Segoe Fluent Icons" });
                remove.FontSize(11.0);
                remove.Height(32.0);
                remove.VerticalAlignment(xaml::VerticalAlignment::Bottom);
                remove.IsEnabled(index != 0 && index + 1 != stops.size() && stops.size() > 2);

                xaml::Automation::AutomationProperties::SetName(
                    remove, resources::GetString(L"RemoveDetentStopName"));

                remove.Click([weak, index](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->RemoveDetentStop(index);
                        }
                    });

                controls::Grid::SetColumn(remove, 1);

                row.Children().Append(remove);

                DetentStopsHost().Items().Append(row);
            }

            AddDetentStopButton().IsEnabled(stops.size() < glass::MaximumDetentStops);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the stops.")
    }

    _Use_decl_annotations_
    void EditorWindow::SetDetentStop(size_t index, double value)
    {
        if (TryEditSelectedMessage([index, value](glass::ControlMessage& message)
            {
                if (index >= message.Detents.Stops.size() ||
                    message.Detents.Stops[index] == value)
                {
                    return false;
                }

                message.Detents.Stops[index] = value;

                return true;
            }))
        {
            RebuildSurface();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::RemoveDetentStop(size_t index)
    {
        if (TryEditSelectedMessage([index](glass::ControlMessage& message)
            {
                // The two ends stay. A range with one end missing is not a range.
                if (index == 0 ||
                    index + 1 >= message.Detents.Stops.size() ||
                    message.Detents.Stops.size() <= 2)
                {
                    return false;
                }

                message.Detents.Stops.erase(message.Detents.Stops.begin() + index);

                return true;
            }))
        {
            RebuildSurface();
            RefreshMessageFields();
        }
    }

    _Use_decl_annotations_
    void EditorWindow::OnAddDetentStopClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (TryEditSelectedMessage([](glass::ControlMessage& message)
            {
                auto& stops = message.Detents.Stops;

                if (stops.size() >= glass::MaximumDetentStops)
                {
                    return false;
                }

                if (stops.size() < 2)
                {
                    stops = { 0.0, 1.0 };
                    return true;
                }

                // A new stop lands halfway between the last two, which is somewhere sensible
                // whatever the range is, rather than on top of one that is already there.
                auto const last = stops.size() - 1;

                stops.insert(
                    stops.begin() + last,
                    (stops[last - 1] + stops[last]) / 2.0);

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
