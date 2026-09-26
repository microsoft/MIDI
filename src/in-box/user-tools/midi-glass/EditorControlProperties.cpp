// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The inspector panels that only some kinds of control have: the marks across a travel, the
// picture an image control shows, the keys on a keyboard, the tempo of a clock, and what any
// control listens for.
//
// A control that has none of these shows none of them. A page of settings that do nothing for
// the control in front of you is worse than a shorter page.

#include "pch.h"
#include "EditorWindow.xaml.h"

#include "StringResources.h"
#include "LayoutStore.h"
#include "ControlFactory.h"

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

        constexpr wchar_t const* FeedbackModeKeys[]
        {
            L"FeedbackModeActivity", L"FeedbackModeNotes", L"FeedbackModeControlChanges",
            L"FeedbackModeMessage", L"FeedbackModeTempo", L"FeedbackModeTransport",
        };

        static_assert(std::size(FeedbackModeOrder) == std::size(FeedbackModeKeys));

        constexpr wchar_t const* FeedbackModeCaptionKeys[]
        {
            L"FeedbackModeActivityCaption",
            L"FeedbackModeNotesCaption",
            L"FeedbackModeControlChangesCaption",
            L"FeedbackModeMessageCaption",
            L"FeedbackModeTempoCaption",
            L"FeedbackModeTransportCaption",
        };

        constexpr glass::BackgroundFit PictureFitOrder[]
        {
            glass::BackgroundFit::Uniform,
            glass::BackgroundFit::Fill,
            glass::BackgroundFit::Stretch,
            glass::BackgroundFit::Centered,
            glass::BackgroundFit::Tiled,
        };

        constexpr wchar_t const* PictureFitKeys[]
        {
            L"BackgroundFitUniform", L"BackgroundFitFill", L"BackgroundFitStretch",
            L"BackgroundFitCentered", L"BackgroundFitTiled",
        };

        static_assert(std::size(PictureFitOrder) == std::size(PictureFitKeys));

        constexpr glass::DragAxis DragAxisOrder[]
        {
            glass::DragAxis::Vertical,
            glass::DragAxis::Horizontal,
        };

        constexpr wchar_t const* DragAxisKeys[]
        {
            L"DragAxisVertical", L"DragAxisHorizontal",
        };

        static_assert(std::size(DragAxisOrder) == std::size(DragAxisKeys));

        // Where a spring-return control goes when the finger comes off. Anything that is not
        // one of the three named positions is a number the customer set on the slider above,
        // so it is offered as itself rather than being rounded into one of these.
        constexpr wchar_t const* SpringTargetKeys[]
        {
            L"SpringTargetLowest", L"SpringTargetCenter", L"SpringTargetHighest",
            L"SpringTargetCustom",
        };

        constexpr double SpringTargetValues[]{ 0.0, 0.5, 1.0 };

        template <typename TEnum, size_t Count>
        int32_t IndexOfValue(_In_ TEnum const (&order)[Count], _In_ TEnum value) noexcept
        {
            for (size_t index = 0; index < Count; ++index)
            {
                if (order[index] == value)
                {
                    return static_cast<int32_t>(index);
                }
            }

            return 0;
        }

        // The note a number plays, written the way this app writes notes everywhere else.
        std::wstring NoteName(_In_ int32_t note) noexcept
        {
            static wchar_t const* const names[]
            {
                L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B"
            };

            auto const clamped = std::clamp(note, 0, 127);

            return std::wstring{ names[clamped % 12] } + std::to_wstring((clamped / 12) - 1);
        }

        // Which kinds show which panel. Written once, because a panel that appears for the
        // wrong control is the same defect as one that never appears at all.
        bool DrawsMarks(_In_ glass::ControlKind kind) noexcept
        {
            switch (kind)
            {
            case glass::ControlKind::Knob:
            case glass::ControlKind::Encoder:
            case glass::ControlKind::Fader:
            case glass::ControlKind::Meter:
            case glass::ControlKind::XYPad:
            case glass::ControlKind::Joystick:
            case glass::ControlKind::Ribbon:
                return true;

            default:
                return false;
            }
        }

        bool ShowsAPicture(_In_ glass::ControlKind kind) noexcept
        {
            return kind == glass::ControlKind::Image || kind == glass::ControlKind::Panel;
        }

        bool IsDraggedInALine(_In_ glass::ControlKind kind) noexcept
        {
            return kind == glass::ControlKind::Knob || kind == glass::ControlKind::Encoder;
        }
    }

    // ------------------------------------------------------------- filling them in

    void EditorWindow::BuildControlPropertyChoices()
    {
        try
        {
            for (auto const* const key : FeedbackModeKeys)
            {
                FeedbackModeCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : PictureFitKeys)
            {
                PictureFitCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : DragAxisKeys)
            {
                DragAxisCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : SpringTargetKeys)
            {
                SpringTargetCombo().Items().Append(box_value(resources::GetString(key)));
            }

            // The message kinds a control can be driven by. A subset of what it can send:
            // nothing on the surface follows a system exclusive dump or a page change.
            for (auto const* const key : { L"MessageControlChange", L"MessageNote",
                L"MessagePitchBend", L"MessageChannelPressure" })
            {
                FeedbackKindCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : { L"DetentModeContinuous", L"DetentModeEvenSteps",
                L"DetentModeList" })
            {
                DetentModeCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (int32_t channel = 1; channel <= 16; ++channel)
            {
                FeedbackChannelCombo().Items().Append(box_value(
                    resources::FormatString(L"ChannelNumberFormat", std::to_wstring(channel))));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to build the control property choices.")
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshKindPanels(glass::Control const& control)
    {
        try
        {
            auto const show = [](xaml::UIElement const& element, bool visible)
                {
                    element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
                };

            // ---- marks and stops ----

            auto const marks = DrawsMarks(control.Kind);

            show(TicksPanel(), marks);

            if (marks)
            {
                TicksShowCheck().IsChecked(control.Ticks.Show);
                TickCountBox().Value(control.Ticks.Count);
                TickCountBox().IsEnabled(control.Ticks.Show);

                auto const stops = glass::DetentStopCount(control);

                TicksCaption().Text(winrt::hstring{ stops > 1
                    ? resources::FormatString(L"TicksFollowStopsFormat", std::to_wstring(stops))
                    : resources::GetString(L"TicksGridCaption") });

                // Nothing to print when the control is smooth all the way.
                ShowDetentValuesCheck().IsEnabled(stops > 1);
                ShowDetentValuesCheck().IsChecked(control.ShowDetentValues);
            }

            // ---- picture ----

            auto const picture = ShowsAPicture(control.Kind);

            show(PicturePanel(), picture);

            if (picture)
            {
                auto const& image = control.Image;

                PictureCaption().Text(winrt::hstring{ image.IsEmpty()
                    ? std::wstring{ resources::GetString(L"PictureNoneCaption") }
                    : image.FileName });

                RemovePictureButton().IsEnabled(!image.IsEmpty());
                PictureFitCombo().SelectedIndex(IndexOfValue(PictureFitOrder, image.Fit));
                PictureOpacitySlider().Value(image.Opacity * 100.0);
                PictureLoopsCheck().IsChecked(image.Loops);
                PictureLoopsCheck().IsEnabled(glass::IsVideoFileName(image.FileName));

                PictureZoomSlider().Value(image.Zoom * 100.0);
                PictureCenterXSlider().Value(image.CenterX * 100.0);
                PictureCenterYSlider().Value(image.CenterY * 100.0);
            }

            // ---- keys ----

            auto const keys = control.Kind == glass::ControlKind::PianoKeyboard;

            show(KeyboardPanel(), keys);

            if (keys)
            {
                auto const& spec = control.Keyboard;

                KeyCountBox().Value(spec.KeyCount);
                LowestNoteBox().Value(spec.LowestNote);
                WhiteKeyColorBox().Text(winrt::hstring{ spec.WhiteKeyColor });
                BlackKeyColorBox().Text(winrt::hstring{ spec.BlackKeyColor });
                PressedKeyColorBox().Text(winrt::hstring{ spec.PressedKeyColor });
                KeyVelocityCheck().IsChecked(spec.VelocityFromKeyPosition);

                auto const highest = std::clamp(spec.LowestNote + spec.KeyCount - 1, 0, 127);

                KeyboardRangeCaption().Text(winrt::hstring{ resources::FormatString(
                    L"KeyboardRangeFormat", NoteName(spec.LowestNote), NoteName(highest)) });
            }

            // ---- how it is dragged ----

            auto const dragged = IsDraggedInALine(control.Kind);

            show(DragAxisPanel(), dragged);

            if (dragged)
            {
                DragAxisCombo().SelectedIndex(IndexOfValue(DragAxisOrder, control.Drag));
            }

            // ---- how hard it was hit ----
            //
            // A pad and a button are the same control until this is on. It is what makes a pad
            // worth having its own entry in the palette.
            auto const padded = control.Kind == glass::ControlKind::Pad;

            show(PadVelocityPanel(), padded);

            if (padded)
            {
                PadVelocityCheck().IsChecked(control.VelocityFromTouch);
            }

            // ---- the clock ----

            auto const clock = control.Kind == glass::ControlKind::BeatClock;

            show(ClockPanel(), clock);

            if (clock)
            {
                ClockBpmBox().Value(control.Clock.BeatsPerMinute);
                ClockLowestBpmBox().Value(control.Clock.LowestBeatsPerMinute);
                ClockHighestBpmBox().Value(control.Clock.HighestBeatsPerMinute);
                ClockStartsRunningCheck().IsChecked(control.Clock.StartsRunning);
                ClockSendsTransportCheck().IsChecked(control.Clock.SendsTransport);

                RefreshTempoSourceChoices(control);
            }

            // ---- where it springs back to ----

            SpringTargetCombo().IsEnabled(control.ReturnsToDefault);

            auto springIndex = static_cast<int32_t>(std::size(SpringTargetKeys)) - 1;

            for (size_t index = 0; index < std::size(SpringTargetValues); ++index)
            {
                if (std::abs(control.DefaultValue - SpringTargetValues[index]) < 0.0001)
                {
                    springIndex = static_cast<int32_t>(index);
                    break;
                }
            }

            SpringTargetCombo().SelectedIndex(springIndex);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show this control's own properties.")
    }

    // Every knob and fader on the layout, so a clock can be told where to get its tempo.
    _Use_decl_annotations_
    void EditorWindow::RefreshTempoSourceChoices(glass::Control const& control)
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            ClockSourceCombo().Items().Clear();
            m_tempoSourceIds.clear();

            ClockSourceCombo().Items().Append(box_value(resources::GetString(L"ClockSourceNone")));
            m_tempoSourceIds.push_back({});

            auto selected = 0;

            for (auto const& page : m_editor.Document().Pages)
            {
                for (auto const& candidate : page.Controls)
                {
                    // A clock taking its tempo from itself would feed back.
                    if (candidate.Id == control.Id)
                    {
                        continue;
                    }

                    if (candidate.Kind != glass::ControlKind::Knob &&
                        candidate.Kind != glass::ControlKind::Encoder &&
                        candidate.Kind != glass::ControlKind::Fader &&
                        candidate.Kind != glass::ControlKind::Ribbon)
                    {
                        continue;
                    }

                    if (candidate.Id == control.Clock.TempoControlId)
                    {
                        selected = static_cast<int32_t>(m_tempoSourceIds.size());
                    }

                    ClockSourceCombo().Items().Append(box_value(winrt::hstring{
                        candidate.Label.empty() ? candidate.Id : candidate.Label }));

                    m_tempoSourceIds.push_back(candidate.Id);
                }
            }

            ClockSourceCombo().SelectedIndex(selected);

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to list the controls a clock can follow.")
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshFeedbackPanel(glass::Control const& control)
    {
        try
        {
            auto const& feedback = control.Feedback;

            FeedbackEnabledSwitch().IsOn(feedback.Enabled);
            FeedbackFieldsPanel().IsEnabled(feedback.Enabled);

            auto const modeIndex = IndexOfValue(FeedbackModeOrder, feedback.Mode);

            FeedbackModeCombo().SelectedIndex(modeIndex);
            FeedbackModeCaption().Text(resources::GetString(FeedbackModeCaptionKeys[modeIndex]));

            RefreshFeedbackDeviceChoices(feedback.DeviceName);
            RefreshFeedbackGroupChoices(feedback.GroupIndex);

            FeedbackChannelCombo().SelectedIndex(std::clamp(feedback.ChannelIndex, 0, 15));
            FeedbackMatchChannelCheck().IsChecked(feedback.MatchesChannel);
            FeedbackNumberBox().Value(static_cast<double>(feedback.Number));
            FeedbackHoldBox().Value(feedback.HoldMilliseconds);

            static constexpr glass::MessageKind FeedbackKinds[]
            {
                glass::MessageKind::ControlChange,
                glass::MessageKind::Note,
                glass::MessageKind::PitchBend,
                glass::MessageKind::ChannelPressure,
            };

            FeedbackKindCombo().SelectedIndex(IndexOfValue(FeedbackKinds, feedback.Kind));

            auto const message = feedback.Mode == glass::FeedbackMode::Message;
            auto const tempo = feedback.Mode == glass::FeedbackMode::Tempo;

            FeedbackMessagePanel().Visibility(
                message ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            FeedbackTempoPanel().Visibility(
                tempo ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // A channel is only a filter for the modes that watch a category of message; one
            // specific message names its channel outright.
            FeedbackMatchChannelCheck().Visibility(
                feedback.Mode == glass::FeedbackMode::AnyActivity ||
                feedback.Mode == glass::FeedbackMode::Notes ||
                feedback.Mode == glass::FeedbackMode::ControlChanges
                    ? xaml::Visibility::Visible
                    : xaml::Visibility::Collapsed);

            FeedbackChannelCombo().IsEnabled(!tempo);

            if (tempo)
            {
                RefreshBeatSourceChoices(control);
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show what this control listens for.")
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshFeedbackDeviceChoices(std::wstring const& selectedName)
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            FeedbackDeviceCombo().Items().Clear();
            m_feedbackDeviceNames.clear();

            FeedbackDeviceCombo().Items().Append(
                box_value(resources::GetString(L"FeedbackAnyDevice")));
            m_feedbackDeviceNames.push_back({});

            auto selected = 0;

            for (auto const& device : m_editor.Document().Devices)
            {
                if (device.Name == selectedName)
                {
                    selected = static_cast<int32_t>(m_feedbackDeviceNames.size());
                }

                FeedbackDeviceCombo().Items().Append(box_value(winrt::hstring{ device.Name }));
                m_feedbackDeviceNames.push_back(device.Name);
            }

            FeedbackDeviceCombo().SelectedIndex(selected);

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to list the devices a control can listen to.")
    }

    _Use_decl_annotations_
    void EditorWindow::RefreshFeedbackGroupChoices(int32_t selectedGroup)
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            FeedbackGroupCombo().Items().Clear();

            FeedbackGroupCombo().Items().Append(box_value(resources::GetString(L"GroupAny")));

            for (int32_t group = 1; group <= glass::MaximumGroupCount; ++group)
            {
                FeedbackGroupCombo().Items().Append(box_value(
                    resources::FormatString(L"GroupNumberFormat", std::to_wstring(group))));
            }

            FeedbackGroupCombo().SelectedIndex(
                selectedGroup == glass::AllGroups
                    ? 0
                    : std::clamp(selectedGroup, 0, glass::MaximumGroupCount - 1) + 1);

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to list the groups a control can listen to.")
    }

    // Every clock generator on the layout, plus "whatever arrives from the device".
    _Use_decl_annotations_
    void EditorWindow::RefreshBeatSourceChoices(glass::Control const& control)
    {
        try
        {
            auto const previous = m_updatingInspector;
            m_updatingInspector = true;

            FeedbackTempoCombo().Items().Clear();
            m_beatSourceIds.clear();

            FeedbackTempoCombo().Items().Append(
                box_value(resources::GetString(L"BeatSourceIncoming")));
            m_beatSourceIds.push_back({});

            auto selected = 0;

            for (auto const& page : m_editor.Document().Pages)
            {
                for (auto const& candidate : page.Controls)
                {
                    if (candidate.Kind != glass::ControlKind::BeatClock)
                    {
                        continue;
                    }

                    if (candidate.Id == control.Feedback.TempoControlId)
                    {
                        selected = static_cast<int32_t>(m_beatSourceIds.size());
                    }

                    FeedbackTempoCombo().Items().Append(box_value(winrt::hstring{
                        candidate.Label.empty() ? candidate.Id : candidate.Label }));

                    m_beatSourceIds.push_back(candidate.Id);
                }
            }

            FeedbackTempoCombo().SelectedIndex(selected);

            m_updatingInspector = previous;
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to list the clocks a control can follow.")
    }
}
