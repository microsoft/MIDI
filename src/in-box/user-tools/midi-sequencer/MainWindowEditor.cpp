// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    namespace
    {
        constexpr double MinimumEditorHeight = 140.0;
        constexpr double MinimumArrangeHeight = 220.0;
        constexpr auto DoubleClickTime = std::chrono::milliseconds{ 450 };

        // "41,287 (63%)": the value as stored or as MIDI 1.0, with how loud that is.
        std::wstring VelocityText(uint16_t velocity, seq::ValueDisplay display)
        {
            auto const percent = static_cast<int32_t>(std::lround(static_cast<double>(velocity) * 100.0 / 65535.0));

            switch (display)
            {
            case seq::ValueDisplay::Midi1:
                return std::format(L"{} ({}%)", seq::ScaleDown(velocity, 16, 7), percent);

            case seq::ValueDisplay::Percent:
                return std::format(L"{:.1f}%", static_cast<double>(velocity) * 100.0 / 65535.0);

            default:
            {
                auto digits = std::to_wstring(velocity);

                for (auto at = static_cast<int32_t>(digits.size()) - 3; at > 0; at -= 3)
                {
                    digits.insert(static_cast<size_t>(at), 1, L',');
                }

                return std::format(L"{} ({}%)", digits, percent);
            }
            }
        }
    }

    _Use_decl_annotations_
    void MainWindow::OpenClipInEditor(std::wstring const& clipId, std::wstring const& trackId) noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, clipId);

            if (clip == nullptr)
            {
                return;
            }

            auto const changed = m_editorClipId != clipId;

            m_editorClipId = clipId;
            m_editorTrackId = trackId;

            EditorPanel().Visibility(xaml::Visibility::Visible);

            RefreshEditor();

            if (changed && m_editorCanvas != nullptr && m_editorCanvas.ActualHeight() > 1)
            {
                m_roll.FitToClip(static_cast<float>(m_editorCanvas.ActualWidth()), static_cast<float>(m_editorCanvas.ActualHeight()));
            }

            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open a clip in the editor.")
    }

    void MainWindow::CloseEditor() noexcept
    {
        try
        {
            if (m_auditioning.has_value())
            {
                AuditionNote(*m_auditioning, false);
                m_auditioning.reset();
            }

            m_editorClipId.clear();
            m_editorTrackId.clear();
            m_roll.SetClip(nullptr, seq::Color{});
            EditorPanel().Visibility(xaml::Visibility::Collapsed);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to close the editor.")
    }

    void MainWindow::RefreshEditor() noexcept
    {
        try
        {
            if (m_editorClipId.empty())
            {
                return;
            }

            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                CloseEditor();
                return;
            }

            auto const track = EditorTrack();
            auto const color = clip->Color.has_value() ? seq::FromRgb(*clip->Color) : seq::FromRgb(track != nullptr ? track->Color : 0x60CDFF);

            // The clip list may have moved since the last edit, so the editor always gets the
            // clip again from the sequence.
            m_roll.SetClip(clip, color);
            m_roll.SetMeter(&m_doc.Meter);

            UpdateEditorHeader();
            UpdateNoteInspector();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to refresh the editor.")
    }

    seq::Track const* MainWindow::EditorTrack() const noexcept
    {
        if (m_editorTrackId.empty())
        {
            return nullptr;
        }

        return seq::FindTrack(m_doc, m_editorTrackId);
    }

    void MainWindow::UpdateEditorHeader() noexcept
    {
        try
        {
            auto const clip = seq::FindClip(m_doc, m_editorClipId);

            if (clip == nullptr)
            {
                return;
            }

            auto const track = EditorTrack();
            auto const color = clip->Color.has_value() ? seq::FromRgb(*clip->Color) : seq::FromRgb(track != nullptr ? track->Color : 0x60CDFF);

            EditorClipName().Text(winrt::hstring{ clip->Name });

            for (auto const& child : EditorKindMark().Children())
            {
                if (auto const shape = child.try_as<shapes::Shape>())
                {
                    shape.Fill(BrushFor(color));
                }
            }

            switch (clip->Kind)
            {
            case seq::ClipKind::Pattern:
                EditorKindChip().Text(res::GetString(L"ClipKindPattern"));
                break;

            case seq::ClipKind::Generator:
                EditorKindChip().Text(res::GetString(L"ClipKindGenerator"));
                break;

            default:
                EditorKindChip().Text(res::GetString(L"ClipKindNotes"));
                break;
            }

            // Bars and beats, in the meter at the start.
            auto const& meter = m_doc.Meter.empty() ? seq::MeterChange{} : m_doc.Meter.front();
            auto const perBar = seq::TicksPerBar(meter);
            auto const perBeat = seq::TicksPerBeat(meter);
            auto const bars = clip->Length / perBar;
            auto const beats = (clip->Length % perBar) / perBeat;

            winrt::hstring length{};

            if (beats == 0)
            {
                length = res::FormatString(bars == 1 ? L"ClipLengthOneBarFormat" : L"ClipLengthBarsFormat", bars);
            }
            else
            {
                length = res::FormatString(L"ClipLengthBarsBeatsFormat", bars, beats);
            }

            if (clip->Loop)
            {
                length = res::FormatString(L"ClipLoopsFormat", std::wstring{ length });
            }

            EditorLengthChip().Text(length);

            auto const uses = seq::CountClipUses(m_doc, clip->Id);
            EditorUsesChip().Visibility(uses > 1 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            EditorUsesText().Text(res::FormatString(L"ClipUsesFormat", uses));

            EditorDrawToggle().IsChecked(m_roll.Tool() == seq::RollTool::Draw);

            auto const grid = SnapGrid();
            EditorQuantizeButton().Content(winrt::box_value(grid > 0 ? res::GetString(L"EditorQuantizeToGrid") : res::GetString(L"EditorQuantizeSixteenths")));

            auto const destination = track != nullptr ? DescribeDestination(*track) : std::wstring{};
            auto const keyboardName = res::FormatString(L"EditorKeyboardNameFormat", destination);
            automation::AutomationProperties::SetName(EditorKeyboardButton(), keyboardName);
            controls::ToolTipService::SetToolTip(EditorKeyboardButton(), winrt::box_value(keyboardName));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to update the editor header.")
    }

    void MainWindow::UpdateNoteInspector() noexcept
    {
        try
        {
            auto const body = NoteInspectorBody();
            body.Children().Clear();

            auto const display = seq::AppSettings::Current().ValuesAs();
            auto const& selection = m_roll.Selection();

            auto const addRow = [&](winrt::hstring const& key, std::wstring const& value)
            {
                controls::StackPanel row{};
                row.Orientation(controls::Orientation::Horizontal);
                row.Margin(xaml::Thickness{ 0, 2, 0, 2 });

                controls::TextBlock name{};
                name.Style(RootGrid().Resources().Lookup(winrt::box_value(L"InspectorKeyStyle")).as<xaml::Style>());
                name.Text(key);
                name.Width(92);
                row.Children().Append(name);

                controls::TextBlock text{};
                text.Style(RootGrid().Resources().Lookup(winrt::box_value(L"InspectorValueStyle")).as<xaml::Style>());
                text.Text(winrt::hstring{ value });
                row.Children().Append(text);

                body.Children().Append(row);
            };

            auto const addNote = [&](winrt::hstring const& text)
            {
                controls::TextBlock note{};
                note.Text(text);
                note.FontSize(12);
                note.TextWrapping(xaml::TextWrapping::Wrap);
                note.Foreground(BrushFor(m_palette.Text3));
                note.Margin(xaml::Thickness{ 0, 2, 0, 8 });
                body.Children().Append(note);
            };

            if (selection.empty())
            {
                NoteInspectorTitle().Text(res::GetString(L"NoteInspectorNone"));
                addNote(res::GetString(L"NoteInspectorHint"));
                return;
            }

            if (selection.size() == 1)
            {
                auto const& note = selection.front();

                NoteInspectorTitle().Text(res::GetString(L"NoteInspectorOne"));
                addRow(res::GetString(L"NoteFieldNote"), seq::NoteLabel(note.Number));
                addRow(res::GetString(L"NoteFieldStart"), seq::PositionLabel(m_doc.Meter, note.Tick));
                addRow(res::GetString(L"NoteFieldLength"), seq::LengthLabel(note.Length));
                addRow(res::GetString(L"NoteFieldVelocity"), VelocityText(note.Velocity, display));
                addRow(res::GetString(L"NoteFieldRelease"), VelocityText(note.ReleaseVelocity, display));
                addRow(res::GetString(L"NoteFieldChannel"), std::to_wstring(static_cast<uint32_t>(note.Channel) + 1));
                addRow(res::GetString(L"NoteFieldChance"), std::to_wstring(note.Chance) + L"%");

                if (note.AttributeType != 0)
                {
                    addRow(res::GetString(L"NoteFieldAttribute"), std::format(L"{} \u00B7 0x{:04X}", note.AttributeType, note.AttributeData));
                }

                return;
            }

            NoteInspectorTitle().Text(res::FormatString(L"NoteInspectorManyFormat", selection.size()));

            auto lowest = selection.front().Number;
            auto highest = selection.front().Number;
            auto quietest = selection.front().Velocity;
            auto loudest = selection.front().Velocity;
            auto start = selection.front().Tick;
            auto end = selection.front().Tick + selection.front().Length;

            for (auto const& note : selection)
            {
                lowest = std::min(lowest, note.Number);
                highest = std::max(highest, note.Number);
                quietest = std::min(quietest, note.Velocity);
                loudest = std::max(loudest, note.Velocity);
                start = std::min(start, note.Tick);
                end = std::max(end, note.Tick + note.Length);
            }

            addRow(res::GetString(L"NoteFieldRange"), seq::NoteLabel(lowest) + L" \u2013 " + seq::NoteLabel(highest));
            addRow(res::GetString(L"NoteFieldStart"), seq::PositionLabel(m_doc.Meter, start));
            addRow(res::GetString(L"NoteFieldLength"), seq::LengthLabel(end - start));
            addRow(res::GetString(L"NoteFieldVelocity"), quietest == loudest ? VelocityText(quietest, display) : VelocityText(quietest, display) + L" \u2013 " + VelocityText(loudest, display));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the selected notes.")
    }

    _Use_decl_annotations_
    void MainWindow::ApplyRollEdit(std::optional<seq::RollEdit> edit) noexcept
    {
        try
        {
            if (!edit.has_value() || (edit->Removed.empty() && edit->Added.empty()) || m_editorClipId.empty())
            {
                InvalidateEditor();
                return;
            }

            if (m_readOnly)
            {
                ShowMessage(res::GetString(L"ReadOnlyEdit"));
            }

            auto added = edit->Added;

            seq::ChangeList changes{};
            changes.push_back(seq::MakeNoteChange(m_editorClipId, std::move(edit->Removed), std::move(edit->Added)));

            m_undo.ApplyAndCommit(m_doc, edit->Name.empty() ? std::wstring{ res::GetString(L"UndoEditNotes") } : edit->Name, std::move(changes));
            DocumentChanged();

            m_roll.SetSelection(std::move(added));
            UpdateNoteInspector();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change the notes.")
    }

    void MainWindow::InvalidateEditor() noexcept
    {
        try
        {
            if (m_editorCanvas == nullptr || m_editorClipId.empty())
            {
                return;
            }

            // The playhead in the clip: where the timeline is inside a placement of it on this
            // track, or how far a launched copy has got.
            int64_t playhead{ -1 };

            if (auto const clip = seq::FindClip(m_doc, m_editorClipId); clip != nullptr && clip->Length > 0)
            {
                auto view = m_launchViews.find(m_editorTrackId);

                if (view != m_launchViews.end() && view->second.Mode == seq::TrackPlayMode::Clip)
                {
                    if (view->second.ClipId == m_editorClipId)
                    {
                        playhead = static_cast<int64_t>(view->second.Progress * static_cast<double>(clip->Length));
                    }
                }
                else if (auto const track = EditorTrack(); track != nullptr)
                {
                    for (auto const& placement : track->Timeline)
                    {
                        auto const length = placement.Length > 0 ? placement.Length : clip->Length;

                        if (placement.ClipId == m_editorClipId && m_position >= placement.Tick && m_position < placement.Tick + length)
                        {
                            playhead = (m_position - placement.Tick) % clip->Length;
                            break;
                        }
                    }
                }
            }

            m_roll.SetPlayhead(playhead);
            m_editorCanvas.Invalidate();
        }
        catch (...)
        {
        }
    }

    // ---------------------------------------------------------------- pointer and keys

    _Use_decl_annotations_
    void MainWindow::OnEditorPressed(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_roll.HasClip())
            {
                return;
            }

            using winrt::Windows::System::VirtualKeyModifiers;

            m_editorCanvas.Focus(xaml::FocusState::Pointer);

            auto const point = args.GetCurrentPoint(m_editorCanvas);
            auto const position = point.Position();
            auto const properties = point.Properties();
            auto const modifiers = args.KeyModifiers();
            auto const shift = (modifiers & VirtualKeyModifiers::Shift) == VirtualKeyModifiers::Shift;
            auto const control = (modifiers & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control;
            auto const right = properties.IsRightButtonPressed();

            auto const now = std::chrono::steady_clock::now();
            auto const doubleClick = !right && now - m_lastEditorPressTime < DoubleClickTime &&
                std::abs(position.X - m_lastEditorPressPoint.X) < 4 && std::abs(position.Y - m_lastEditorPressPoint.Y) < 4;

            m_lastEditorPressTime = doubleClick ? std::chrono::steady_clock::time_point{} : now;
            m_lastEditorPressPoint = position;

            // The keys on the left play the note.
            if (!right && position.X < seq::PianoRoll::KeysWidth)
            {
                if (auto const key = m_roll.KeyAt(position); key.has_value())
                {
                    m_auditioning = *key;
                    AuditionNote(*key, true);
                    m_editorCanvas.CapturePointer(args.Pointer());
                    args.Handled(true);
                    return;
                }
            }

            if (m_roll.PointerPressed(position, shift, control, right, doubleClick))
            {
                InvalidateEditor();
            }

            m_editorGesture = true;
            m_editorCanvas.CapturePointer(args.Pointer());
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a click in the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorMoved(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            auto const position = args.GetCurrentPoint(m_editorCanvas).Position();

            if (m_auditioning.has_value())
            {
                // Sliding along the keys plays each one.
                auto const key = m_roll.KeyAt(position);

                if (key.has_value() && *key != *m_auditioning)
                {
                    AuditionNote(*m_auditioning, false);
                    m_auditioning = *key;
                    AuditionNote(*key, true);
                }

                return;
            }

            if (m_editorGesture && m_roll.PointerMoved(position))
            {
                InvalidateEditor();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a drag in the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorReleased(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (m_auditioning.has_value())
            {
                AuditionNote(*m_auditioning, false);
                m_auditioning.reset();
                m_editorCanvas.ReleasePointerCaptures();
                return;
            }

            if (!m_editorGesture)
            {
                return;
            }

            m_editorGesture = false;

            auto const position = args.GetCurrentPoint(m_editorCanvas).Position();
            auto edit = m_roll.PointerReleased(position);
            m_editorCanvas.ReleasePointerCaptures();

            if (edit.has_value())
            {
                ApplyRollEdit(std::move(edit));
            }
            else
            {
                UpdateNoteInspector();
                InvalidateEditor();
            }
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish a gesture in the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorWheel(input::PointerRoutedEventArgs const& args)
    {
        try
        {
            using winrt::Windows::System::VirtualKeyModifiers;

            auto const point = args.GetCurrentPoint(m_editorCanvas);
            auto const modifiers = args.KeyModifiers();
            auto const shift = (modifiers & VirtualKeyModifiers::Shift) == VirtualKeyModifiers::Shift ||
                point.Properties().IsHorizontalMouseWheel();
            auto const control = (modifiers & VirtualKeyModifiers::Control) == VirtualKeyModifiers::Control;

            if (m_roll.Wheel(point.Position(), point.Properties().MouseWheelDelta(), shift, control,
                static_cast<float>(m_editorCanvas.ActualWidth()), static_cast<float>(m_editorCanvas.ActualHeight())))
            {
                InvalidateEditor();
            }

            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to scroll the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorKeyDown(input::KeyRoutedEventArgs const& args)
    {
        try
        {
            using winrt::Windows::System::VirtualKey;
            using winrt::Windows::System::VirtualKeyModifiers;

            if (!m_roll.HasClip())
            {
                return;
            }

            auto const window = winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Control);
            auto const control = (window & winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;
            auto const shiftState = winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Shift);
            auto const shift = (shiftState & winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;

            auto const grid = std::max<int64_t>(SnapGrid(), 1);
            std::optional<seq::RollEdit> edit{};
            auto handled = true;

            switch (args.Key())
            {
            case VirtualKey::Delete:
            case VirtualKey::Back:
                edit = m_roll.DeleteSelection();
                break;

            case VirtualKey::Left:
                edit = m_roll.MoveSelection(-grid, 0);
                break;

            case VirtualKey::Right:
                edit = m_roll.MoveSelection(grid, 0);
                break;

            case VirtualKey::Up:
                edit = m_roll.MoveSelection(0, shift ? 12 : 1);
                break;

            case VirtualKey::Down:
                edit = m_roll.MoveSelection(0, shift ? -12 : -1);
                break;

            case VirtualKey::A:
                if (control)
                {
                    m_roll.SelectAll();
                    UpdateNoteInspector();
                    InvalidateEditor();
                }
                else
                {
                    handled = false;
                }
                break;

            case VirtualKey::Escape:
                m_roll.ClearSelection();
                UpdateNoteInspector();
                InvalidateEditor();
                break;

            case VirtualKey::Q:
                edit = m_roll.QuantizeSelection(SnapGrid() > 0 ? SnapGrid() : 240);
                break;

            case VirtualKey::B:
                m_roll.SetTool(m_roll.Tool() == seq::RollTool::Draw ? seq::RollTool::Select : seq::RollTool::Draw);
                EditorDrawToggle().IsChecked(m_roll.Tool() == seq::RollTool::Draw);
                break;

            default:
                handled = false;
                break;
            }

            if (edit.has_value())
            {
                ApplyRollEdit(std::move(edit));
            }

            args.Handled(handled);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to handle a key in the editor.")
    }

    // ---------------------------------------------------------------- header buttons

    _Use_decl_annotations_
    void MainWindow::OnEditorDrawClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        m_roll.SetTool(EditorDrawToggle().IsChecked().GetBoolean() ? seq::RollTool::Draw : seq::RollTool::Select);
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorQuantizeClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_roll.Selection().empty())
            {
                m_roll.SelectAll();
            }

            ApplyRollEdit(m_roll.QuantizeSelection(SnapGrid() > 0 ? SnapGrid() : 240));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to quantize.")
    }

    _Use_decl_annotations_
    void MainWindow::OnTransposeClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = sender.try_as<controls::MenuFlyoutItem>();

            if (item == nullptr)
            {
                return;
            }

            auto const semitones = std::stoi(std::wstring{ winrt::unbox_value_or<winrt::hstring>(item.Tag(), L"0") });

            if (m_roll.Selection().empty())
            {
                m_roll.SelectAll();
            }

            ApplyRollEdit(m_roll.TransposeSelection(semitones));
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to transpose.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorKeyboardClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const track = EditorTrack();

            if (track == nullptr)
            {
                return;
            }

            LaunchKeyboard(m_directory->ResolveId(track->Destination.Endpoint), track->Destination.Group, track->Destination.Channel);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to open MIDI Keyboard.")
    }

    _Use_decl_annotations_
    void MainWindow::OnValuesAsChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            auto const index = ValuesAsCombo().SelectedIndex();

            if (index < 0)
            {
                return;
            }

            auto const value = static_cast<seq::ValueDisplay>(index);
            seq::AppSettings::Current().ValuesAs(value);
            m_roll.SetValuesAs(value);
            UpdateNoteInspector();
            InvalidateEditor();
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to change how values are shown.")
    }

    _Use_decl_annotations_
    void MainWindow::OnEditorCloseClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        CloseEditor();
    }

    // ---------------------------------------------------------------- the splitter

    _Use_decl_annotations_
    void MainWindow::OnSplitterPressed(foundation::IInspectable const&, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            m_splitterDragging = true;
            m_splitterStartY = args.GetCurrentPoint(RootGrid()).Position().Y;
            m_splitterStartHeight = EditorPanel().ActualHeight();
            EditorSplitter().CapturePointer(args.Pointer());
            args.Handled(true);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to start resizing the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterMoved(foundation::IInspectable const&, input::PointerRoutedEventArgs const& args)
    {
        try
        {
            if (!m_splitterDragging)
            {
                return;
            }

            auto const y = args.GetCurrentPoint(RootGrid()).Position().Y;
            auto const maximum = std::max(MinimumEditorHeight, RootGrid().ActualHeight() - MinimumArrangeHeight - 120.0);
            auto const height = std::clamp(m_splitterStartHeight - (y - m_splitterStartY), MinimumEditorHeight, maximum);

            EditorPanel().Height(height);
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to resize the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterReleased(foundation::IInspectable const&, input::PointerRoutedEventArgs const&)
    {
        try
        {
            if (!m_splitterDragging)
            {
                return;
            }

            m_splitterDragging = false;
            EditorSplitter().ReleasePointerCaptures();
            seq::AppSettings::Current().EditorHeight(EditorPanel().Height());
        }
        MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to finish resizing the editor.")
    }

    _Use_decl_annotations_
    void MainWindow::OnSplitterCaptureLost(foundation::IInspectable const& sender, input::PointerRoutedEventArgs const& args)
    {
        OnSplitterReleased(sender, args);
    }
}
