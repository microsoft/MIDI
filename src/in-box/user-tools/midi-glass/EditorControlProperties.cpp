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
#include "PadGrid.h"
#include "InputRules.h"

#include <shobjidl.h>
#include <cwctype>
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
            glass::DragAxis::Circular,
        };

        constexpr wchar_t const* DragAxisKeys[]
        {
            L"DragAxisVertical", L"DragAxisHorizontal", L"DragAxisCircular",
        };

        static_assert(std::size(DragAxisOrder) == std::size(DragAxisKeys));

        constexpr wchar_t const* LfoWaveKeys[]
        {
            L"LfoWaveSine", L"LfoWaveTriangle", L"LfoWaveSquare", L"LfoWaveRampUp",
            L"LfoWaveRampDown", L"LfoWaveWhite", L"LfoWavePink", L"LfoWaveBrown", L"LfoWaveBlue",
        };

        static_assert(std::size(glass::LfoWaveOrder) == std::size(LfoWaveKeys));

        constexpr wchar_t const* LfoRateKeys[]
        {
            L"LfoRateSixteenth", L"LfoRateEighth", L"LfoRateQuarter", L"LfoRateDottedQuarter",
            L"LfoRateHalf", L"LfoRateDottedHalf", L"LfoRateBar", L"LfoRateTwoBars",
            L"LfoRateFourBars", L"LfoRateEightBars",
        };

        static_assert(std::size(glass::LfoRateChoices) == std::size(LfoRateKeys));

        constexpr wchar_t const* PadScaleKeys[]
        {
            L"PadScaleMajor", L"PadScaleMinor", L"PadScaleHarmonicMinor", L"PadScaleMelodicMinor",
            L"PadScaleDorian", L"PadScalePhrygian", L"PadScaleLydian", L"PadScaleMixolydian",
            L"PadScaleLocrian", L"PadScaleMajorPentatonic", L"PadScaleMinorPentatonic",
            L"PadScaleBlues", L"PadScaleWholeTone",
        };

        static_assert(std::size(glass::PadScaleOrder) == std::size(PadScaleKeys));

        constexpr wchar_t const* PadNoteNamesKeys[]
        {
            L"PadNamesCenter", L"PadNamesTop", L"PadNamesBottom", L"PadNamesTopLeft",
            L"PadNamesTopRight", L"PadNamesBottomLeft", L"PadNamesBottomRight", L"PadNamesHidden",
        };

        static_assert(std::size(glass::PadNoteNamesOrder) == std::size(PadNoteNamesKeys));

        constexpr wchar_t const* PadGlideKeys[]
        {
            L"PadGlideOff", L"PadGlidePortamento", L"PadGlidePerNoteBend",
        };

        constexpr wchar_t const* PadGlideCaptionKeys[]
        {
            L"PadGlideOffCaption", L"PadGlidePortamentoCaption", L"PadGlidePerNoteBendCaption",
        };

        static_assert(std::size(glass::PadGlideOrder) == std::size(PadGlideKeys));
        static_assert(std::size(glass::PadGlideOrder) == std::size(PadGlideCaptionKeys));

        // The named hexagon layouts, then the customer's own.
        constexpr wchar_t const* PadHexLayoutKeys[]
        {
            L"PadHexLayoutWickiHayden", L"PadHexLayoutHarmonicTable", L"PadHexLayoutJanko",
            L"PadHexLayoutCustom",
        };

        static_assert(std::size(glass::HexLayoutOrder) + 1 == std::size(PadHexLayoutKeys));

        // Where each row of square pads starts, from the end of the row below up to an octave.
        constexpr wchar_t const* PadRowKeys[]
        {
            L"PadRowCarryOn", L"PadRowSemitone", L"PadRowWholeTone", L"PadRowMinorThird",
            L"PadRowMajorThird", L"PadRowFourth", L"PadRowTritone", L"PadRowFifth",
            L"PadRowMinorSixth", L"PadRowMajorSixth", L"PadRowMinorSeventh",
            L"PadRowMajorSeventh", L"PadRowOctave",
        };

        static_assert(glass::PadRowChoiceCount == std::size(PadRowKeys));

        // A key as a person picks it: one name for a natural, both names for a black key.
        std::wstring KeyChoiceName(_In_ int32_t root)
        {
            auto const sharp = glass::PadNoteName(60 + root, false);
            auto const flat = glass::PadNoteName(60 + root, true);

            // The octave number is not part of a key.
            auto const letter = [](std::wstring name)
                {
                    while (!name.empty() && (std::iswdigit(name.back()) || name.back() == L'-'))
                    {
                        name.pop_back();
                    }

                    return name;
                };

            return sharp == flat ? letter(sharp) : letter(sharp) + L" / " + letter(flat);
        }

        // A figure a person reads, not a float: 4 rather than 4.000000, 1.5 rather than 1.500000.
        std::wstring TrimNumber(_In_ double value)
        {
            auto text = std::format(L"{:.2f}", value);

            while (!text.empty() && text.back() == L'0')
            {
                text.pop_back();
            }

            if (!text.empty() && text.back() == L'.')
            {
                text.pop_back();
            }

            return text;
        }

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
        std::wstring NoteName(_In_ int32_t note)
        {
            return glass::PadNoteName(std::clamp(note, 0, 127), false);
        }

        // Which kinds show which panel. Written once, because a panel that appears for the
        // wrong control is the same defect as one that never appears at all.
        bool DrawsMarks(_In_ glass::ControlKind kind) noexcept
        {
            switch (kind)
            {
            case glass::ControlKind::Knob:
            case glass::ControlKind::Fader:
            case glass::ControlKind::Meter:
            case glass::ControlKind::XYPad:
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

        bool ChoosesHowItIsDragged(_In_ glass::ControlKind kind) noexcept
        {
            return kind == glass::ControlKind::Knob;
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

            for (auto const* const key : LfoWaveKeys)
            {
                LfoWaveCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : LfoRateKeys)
            {
                LfoRateCombo().Items().Append(box_value(resources::GetString(key)));
            }

            BuildStepsChoices();

            for (auto const* const key : SpringTargetKeys)
            {
                SpringTargetCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : PadScaleKeys)
            {
                PadScaleCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : PadNoteNamesKeys)
            {
                PadNamesCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : PadGlideKeys)
            {
                PadGlideCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : PadHexLayoutKeys)
            {
                PadHexLayoutCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : PadRowKeys)
            {
                PadRowCombo().Items().Append(box_value(resources::GetString(key)));
            }

            PadKeyCombo().Items().Append(box_value(resources::GetString(L"PadKeyNone")));

            for (int32_t root = 0; root < 12; ++root)
            {
                PadKeyCombo().Items().Append(box_value(winrt::hstring{ KeyChoiceName(root) }));
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

            // In the order of the two answers a line's rectangle can give, and of LineEnds.
            for (auto const* const key : { L"LineDirectionAcross", L"LineDirectionDown" })
            {
                LineDirectionCombo().Items().Append(box_value(resources::GetString(key)));
            }

            for (auto const* const key : { L"LineEndsUseTheme", L"LineEndsSquare", L"LineEndsFaded" })
            {
                LineEndsCombo().Items().Append(box_value(resources::GetString(key)));
            }

            // Every color code in the inspector gets the same swatch button beside it. Blank is
            // a real answer on all four: it means the theme decides.
            auto const keyboardEdit = [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->ApplyKeyboardEdit();
                    }
                };

            AttachColorPicker(WhiteKeyColorButton(), WhiteKeyColorBox(), true, keyboardEdit);
            AttachColorPicker(BlackKeyColorButton(), BlackKeyColorBox(), true, keyboardEdit);
            AttachColorPicker(PressedKeyColorButton(), PressedKeyColorBox(), true, keyboardEdit);

            // Blank is a real answer on all four pad colors too: the theme decides.
            auto const padEdit = [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->ApplyPadGridEdit();
                    }
                };

            AttachColorPicker(PadRootColorButton(), PadRootColorBox(), true, padEdit);
            AttachColorPicker(PadInKeyColorButton(), PadInKeyColorBox(), true, padEdit);
            AttachColorPicker(PadOutOfKeyColorButton(), PadOutOfKeyColorBox(), true, padEdit);
            AttachColorPicker(PadPressedColorButton(), PadPressedColorBox(), true, padEdit);

            AttachColorPicker(PictureTintColorButton(), PictureTintBox(), true,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->OnPictureTintChanged(nullptr, nullptr);
                    }
                });

            AttachColorPicker(LineColorButton(), LineColorBox(), true,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->ApplyLineEdit();
                    }
                });

            BuildPicturePanel();
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
                auto const chosen = !image.IsEmpty();

                PictureCaption().Text(winrt::hstring{ chosen
                    ? image.FileName
                    : std::wstring{ resources::GetString(L"PictureNoneCaption") } });

                RemovePictureButton().IsEnabled(chosen);

                // Nothing below changes anything until there is a picture to change. A switched
                // off control dims itself; the headings and labels between them do not, so they
                // are dimmed to match.
                PictureSettings().IsEnabled(chosen);

                if (chosen)
                {
                    PictureSettings().ClearValue(controls::Control::ForegroundProperty());
                }
                else
                {
                    PictureSettings().Foreground(xaml::Application::Current().Resources()
                        .Lookup(box_value(L"TextFillColorDisabledBrush")).as<media::Brush>());
                }

                PictureFitCombo().SelectedIndex(IndexOfValue(PictureFitOrder, image.Fit));
                PictureOpacitySlider().Value(image.Opacity * 100.0);

                // A video's own settings, for a video only. Clicks and the bar belong to an
                // image control: a panel's fill sits behind the controls on the panel.
                auto const isVideo = chosen && glass::IsVideoFileName(image.FileName);

                show(PictureVideoPanel(), isVideo);

                if (isVideo)
                {
                    auto const takesInput = control.Kind == glass::ControlKind::Image;

                    PictureLoopsCheck().IsChecked(image.Loops);
                    PictureAutoPlayCheck().IsChecked(image.AutoPlays);
                    PictureClickToPlayCheck().IsChecked(image.ClickToPlay);
                    PictureScrubberCheck().IsChecked(image.ShowsScrubber);
                    PictureClickToPlayCheck().IsEnabled(takesInput);
                    PictureScrubberCheck().IsEnabled(takesInput);

                    show(PictureVideoInputCaption(), !takesInput);
                }

                PictureZoomSlider().Value(image.Zoom * 100.0);
                PictureTintBox().Text(winrt::hstring{ image.TintColor });
                PictureTintStrengthSlider().Value(image.TintStrength * 100.0);

                RefreshPicturePosition(control);
                RefreshPictureVideoPanel();
            }
            else
            {
                RefreshPictureVideoPanel();
            }

            // ---- switch positions ----

            auto const selector = control.Kind == glass::ControlKind::Switch;

            show(SwitchPanel(), selector);

            if (selector)
            {
                RefreshSwitchPositions(control);
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
                KeyNamesCheck().IsChecked(spec.ShowNoteNames);

                auto const highest = std::clamp(spec.LowestNote + spec.KeyCount - 1, 0, 127);

                KeyboardRangeCaption().Text(winrt::hstring{ resources::FormatString(
                    L"KeyboardRangeFormat", NoteName(spec.LowestNote), NoteName(highest)) });
            }

            // ---- pads ----

            auto const pads = glass::IsPadGrid(control.Kind);

            show(PadGridPanel(), pads);

            if (pads)
            {
                RefreshPadGridPanel(control);
            }

            // ---- where it starts ----
            //
            // A toggle is on or off, so it starts one way or the other. A percentage slider for
            // that gave nobody a way to say "on" short of dragging it past half way.
            auto const onOff = control.Kind == glass::ControlKind::Toggle;

            show(DefaultValueSlider(), !onOff);
            show(StartsOnSwitch(), onOff);

            if (onOff)
            {
                StartsOnSwitch().IsOn(control.DefaultValue >= 0.5);
            }

            // An XY pad and a joystick start at a point, so each axis gets its own slider.
            auto const twoAxes = glass::UsesTwoAxes(control.Kind);

            show(DefaultValueYSlider(), twoAxes);

            DefaultValueSlider().Header(twoAxes
                ? box_value(resources::GetString(L"DefaultValueAcrossHeader"))
                : nullptr);

            xaml::Automation::AutomationProperties::SetName(
                DefaultValueSlider(),
                resources::GetString(twoAxes ? L"DefaultValueAcrossName" : L"DefaultValueName"));

            // ---- how it is dragged ----

            auto const dragged = ChoosesHowItIsDragged(control.Kind);

            show(DragAxisPanel(), dragged);

            if (dragged)
            {
                DragAxisCombo().SelectedIndex(IndexOfValue(DragAxisOrder, control.Drag));
            }

            // ---- how hard it was hit ----
            //
            // A pad and a button are the same control until this is on. It is what makes a pad
            // worth having its own entry in the palette. A grid of pads is pads too.
            auto const padded = control.Kind == glass::ControlKind::Pad || glass::IsPadGrid(control.Kind);

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

            // ---- the sweep ----

            auto const sweep = control.Kind == glass::ControlKind::Lfo;

            show(LfoPanel(), sweep);

            if (sweep)
            {
                auto const& spec = control.Lfo;

                LfoWaveCombo().SelectedIndex(IndexOfValue(glass::LfoWaveOrder, spec.Wave));

                auto rateIndex = -1;

                for (size_t index = 0; index < std::size(glass::LfoRateChoices); ++index)
                {
                    if (std::abs(spec.BeatsPerCycle - glass::LfoRateChoices[index]) < 0.0001)
                    {
                        rateIndex = static_cast<int32_t>(index);
                        break;
                    }
                }

                // A file can carry a figure that is not on the list. The combo is left blank
                // rather than snapped to the nearest, and the caption says what is really set.
                LfoRateCombo().SelectedIndex(rateIndex);

                auto const tempo = std::clamp(
                    m_editor.Document().Tempo.BeatsPerMinute,
                    glass::MinimumBeatsPerMinute,
                    glass::MaximumBeatsPerMinute);

                LfoRateCaption().Text(winrt::hstring{ resources::FormatString(
                    L"LfoRateCaptionFormat",
                    TrimNumber(spec.BeatsPerCycle),
                    TrimNumber(spec.BeatsPerCycle * 60.0 / tempo),
                    TrimNumber(tempo)) });

                LfoLowestSlider().Value(spec.Lowest * 100.0);
                LfoHighestSlider().Value(spec.Highest * 100.0);
                LfoUpdateBox().Value(spec.UpdateIntervalMilliseconds);

                LfoUpdateCaption().Text(winrt::hstring{ resources::FormatString(
                    L"LfoUpdateCaptionFormat",
                    std::to_wstring(std::max(1, 1000 / std::max(1, spec.UpdateIntervalMilliseconds)))) });

                LfoLatchingCheck().IsChecked(spec.Latching);
                LfoStartsRunningCheck().IsChecked(spec.StartsRunning);
                LfoReturnsToRestCheck().IsChecked(spec.ReturnsToRestWhenStopped);
            }

            // ---- the step sequencer ----

            auto const sequencer = control.Kind == glass::ControlKind::Steps;

            show(StepsPanel(), sequencer);

            if (sequencer)
            {
                RefreshStepsPanel(control);
            }

            // ---- the platter ----

            auto const platter = control.Kind == glass::ControlKind::Turntable;

            show(TurntablePanel(), platter);

            if (platter)
            {
                TurntableDegreesBox().Value(control.Turntable.DegreesForFullRange);
                TurntableGripCheck().IsChecked(control.Turntable.ShowsGrip);
            }

            // ---- the line ----

            auto const line = control.Kind == glass::ControlKind::Line;

            show(LinePanel(), line);

            if (line)
            {
                LineDirectionCombo().SelectedIndex(control.Width >= control.Height ? 0 : 1);
                LineThicknessBox().Value(control.Line.Thickness);
                LineColorBox().Text(winrt::hstring{ control.Line.Color });
                LineEndsCombo().SelectedIndex(static_cast<int32_t>(control.Line.Ends));
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

    // The pads on a note pad or hex pad control. Called with the inspector already marked as
    // filling itself in, so nothing set here records an edit.
    _Use_decl_annotations_
    void EditorWindow::RefreshPadGridPanel(glass::Control const& control)
    {
        auto const show = [](xaml::UIElement const& element, bool visible)
            {
                element.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            };

        auto const& spec = control.Pads;
        auto const hex = glass::IsHexPadGrid(control.Kind);
        auto const keyed = spec.KeyRoot != glass::NoKey;

        PadCountBox().Value(spec.PadCount);
        PadSizeBox().Value(spec.PadSize);
        PadStartNoteBox().Value(spec.StartNote);
        PadRightIntervalBox().Value(spec.RightInterval);
        PadUpIntervalBox().Value(spec.RowInterval);

        // Square pads pick where each row starts from a list. Hexagons pick a layout by name, or
        // give both of their intervals.
        show(PadRowCombo(), !hex);
        show(PadHexLayoutCombo(), hex);
        show(PadUpIntervalBox(), hex);

        if (hex)
        {
            auto layoutIndex = static_cast<int32_t>(std::size(glass::HexLayoutOrder));

            for (size_t index = 0; index < std::size(glass::HexLayoutOrder); ++index)
            {
                if (glass::HexLayoutOrder[index].RightInterval == spec.RightInterval &&
                    glass::HexLayoutOrder[index].RowInterval == spec.RowInterval)
                {
                    layoutIndex = static_cast<int32_t>(index);
                    break;
                }
            }

            PadHexLayoutCombo().SelectedIndex(layoutIndex);
        }
        else
        {
            // A file can carry a step past an octave. The list is left blank rather than showing
            // something that is not what the pads do.
            PadRowCombo().SelectedIndex(
                spec.RowInterval >= 0 && spec.RowInterval < glass::PadRowChoiceCount ? spec.RowInterval : -1);
        }

        PadKeyCombo().SelectedIndex(keyed && spec.KeyRoot <= 11 ? spec.KeyRoot + 1 : 0);
        PadScaleCombo().SelectedIndex(IndexOfValue(glass::PadScaleOrder, spec.Scale));
        PadScaleCombo().IsEnabled(keyed);

        PadRootColorBox().Text(winrt::hstring{ spec.RootColor });
        PadInKeyColorBox().Text(winrt::hstring{ spec.InKeyColor });
        PadOutOfKeyColorBox().Text(winrt::hstring{ spec.OutOfKeyColor });
        PadPressedColorBox().Text(winrt::hstring{ spec.PressedColor });

        // With no key every pad is in it, so the root and the notes outside it have no color
        // to set.
        PadRootColorBox().IsEnabled(keyed);
        PadRootColorButton().IsEnabled(keyed);
        PadOutOfKeyColorBox().IsEnabled(keyed);
        PadOutOfKeyColorButton().IsEnabled(keyed);

        PadNamesCombo().SelectedIndex(IndexOfValue(glass::PadNoteNamesOrder, spec.NoteNames));
        PadNameSizeBox().Value(spec.NoteNameSize);
        PadNameSizeBox().IsEnabled(spec.NoteNames != glass::PadNoteNames::Hidden);

        auto const glideIndex = IndexOfValue(glass::PadGlideOrder, spec.Glide);

        PadGlideCombo().SelectedIndex(glideIndex);
        PadGlideCaption().Text(resources::GetString(PadGlideCaptionKeys[glideIndex]));
        PadBendRangeBox().Value(spec.BendRangeSemitones);
        show(PadBendRangeBox(), spec.Glide == glass::PadGlide::PerNoteBend);

        // How the pads actually flowed in this control, worked out the same way the surface
        // draws them, so the caption can never describe a grid that is not on the page.
        auto const layout = glass::LayOutPadGrid(spec, hex, control.Width, control.Height);
        auto const asked = std::clamp(spec.PadSize, glass::MinimumPadSize, glass::MaximumPadSize);

        if (layout.PadWidth + 0.01 < asked)
        {
            PadFlowCaption().Text(resources::FormatString(
                L"PadFlowShrunkFormat",
                std::to_wstring(layout.Columns),
                std::to_wstring(layout.Rows),
                TrimNumber(layout.PadWidth),
                TrimNumber(asked)));
        }
        else
        {
            PadFlowCaption().Text(resources::FormatString(
                L"PadFlowFormat",
                std::to_wstring(layout.Columns),
                std::to_wstring(layout.Rows)));
        }

        auto lowest = 128;
        auto highest = -1;

        for (auto const& cell : layout.Cells)
        {
            if (cell.Note >= 0)
            {
                lowest = std::min(lowest, cell.Note);
                highest = std::max(highest, cell.Note);
            }
        }

        auto const flats = keyed && glass::KeyUsesFlats(spec.KeyRoot, spec.Scale);

        PadRangeCaption().Text(highest >= 0
            ? resources::FormatString(
                L"PadRangeFormat", glass::PadNoteName(lowest, flats), glass::PadNoteName(highest, flats))
            : resources::GetString(L"PadRangeNone"));
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
