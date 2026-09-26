// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, so the unit tests compile it unchanged.

#include "ControlFactory.h"
#include "PageTemplates.h"

#include <algorithm>
#include <set>

namespace glass
{
    namespace
    {
        // Where a new control starts, when nothing on the page suggests otherwise.
        constexpr uint32_t FirstController = 1;
        constexpr uint32_t FirstNote = 36;

        int32_t NextKeyboardOrder(_In_ Page const& page) noexcept
        {
            int32_t highest{ 0 };

            for (auto const& control : page.Controls)
            {
                highest = std::max(highest, control.KeyboardOrder);
            }

            return highest + 1;
        }

        // Spread across the six hue slots as controls are added, so a page built by dropping
        // twelve controls is not twelve of the same color.
        int32_t NextHueSlot(_In_ Page const& page) noexcept
        {
            return static_cast<int32_t>(page.Controls.size() % HueSlotCount);
        }
    }

    std::vector<PaletteEntry> const& Palette() noexcept
    {
        // The art numbers come straight from the design comp, screen 4. Anything marked coming
        // is in the design but not yet in the document model.
        static std::vector<PaletteEntry> const entries
        {
            // ---- Buttons ----
            { ControlKind::Button,  L"PaletteButton",  L"PaletteGroupButtons", L'\uE7C4',
                { PaletteArtShape::Rectangle, 22, 14, 3, 0.80, 0.00 } },
            { ControlKind::Toggle,  L"PaletteToggle",  L"PaletteGroupButtons", L'\uE73E',
                { PaletteArtShape::Rectangle, 22, 14, 3, 0.90, 0.35 } },
            { ControlKind::Pad,     L"PalettePad",     L"PaletteGroupButtons", L'\uE71D',
                { PaletteArtShape::Rectangle, 18, 18, 4, 0.80, 0.18 } },
            { ControlKind::PageTab, L"PalettePageTab", L"PaletteGroupButtons", L'\uE8A5',
                { PaletteArtShape::Rectangle, 22, 14, 7, 0.80, 0.15 } },
            { ControlKind::Knob,    L"PaletteRadio",   L"PaletteGroupButtons", L'\uECCA',
                { PaletteArtShape::Rectangle, 22, 14, 7, 0.80, 0.00 }, true },
            { ControlKind::Knob,    L"PaletteStepper", L"PaletteGroupButtons", L'\uE8CB',
                { PaletteArtShape::Sample, 22, 14, 0, 0.00, 0.00, L"1" }, true },

            // ---- Knobs and faders ----
            { ControlKind::Knob,    L"PaletteKnob",    L"PaletteGroupKnobs",   L'\uEA3A',
                { PaletteArtShape::Ellipse, 17, 17, 0, 0.85, 0.00 } },
            { ControlKind::Encoder, L"PaletteEncoder", L"PaletteGroupKnobs",   L'\uE9F5',
                { PaletteArtShape::Ellipse, 17, 17, 0, 0.45, 0.00 } },
            { ControlKind::Turntable, L"PaletteTurntable", L"PaletteGroupKnobs", L'\uE93C',
                { PaletteArtShape::Ellipse, 19, 19, 0, 0.25, 0.00 } },
            { ControlKind::Fader,   L"PaletteFader",   L"PaletteGroupKnobs",   L'\uE9E9',
                { PaletteArtShape::Rectangle, 5, 19, 3, 0.00, 0.70 } },
            { ControlKind::Fader,   L"PaletteWheel",   L"PaletteGroupKnobs",   L'\uE9E9',
                { PaletteArtShape::Rectangle, 10, 19, 3, 0.80, 0.00 }, true },
            { ControlKind::Fader,   L"PaletteRange",   L"PaletteGroupKnobs",   L'\uE9E9',
                { PaletteArtShape::Rectangle, 20, 6, 3, 0.80, 0.00 }, true },

            // ---- Two axis ----
            { ControlKind::XYPad,   L"PaletteXYPad",   L"PaletteGroupTwoAxis", L'\uE80A',
                { PaletteArtShape::Rectangle, 18, 18, 3, 0.80, 0.00 } },
            { ControlKind::Joystick, L"PaletteJoystick", L"PaletteGroupTwoAxis", L'\uE80A',
                { PaletteArtShape::Ellipse, 18, 18, 0, 0.80, 0.00 } },
            { ControlKind::Ribbon,  L"PaletteRibbon",  L"PaletteGroupTwoAxis", L'\uE80A',
                { PaletteArtShape::Rectangle, 20, 8, 4, 0.00, 0.30 } },

            // ---- Generators ----
            { ControlKind::BeatClock, L"PaletteBeatClock", L"PaletteGroupGenerators", L'\uE916',
                { PaletteArtShape::Glyph, 18, 18, 0, 0.00, 0.00, L"\uE916" } },
            { ControlKind::Lfo,     L"PaletteLfo",     L"PaletteGroupGenerators", L'\uE9E9',
                { PaletteArtShape::Wave, 20, 12, 0, 0.85, 0.00 } },
            { ControlKind::Knob,    L"PaletteSteps",   L"PaletteGroupGenerators", L'\uE8FD',
                { PaletteArtShape::HorizontalBars, 20, 10, 0, 0.00, 0.80 }, true },

            // ---- Keys ----
            { ControlKind::PianoKeyboard, L"PaletteKeyboard", L"PaletteGroupKeys", L'\uEC4F',
                { PaletteArtShape::Keys, 26, 14, 1, 0.00, 0.00 } },

            // ---- Feedback and text ----
            { ControlKind::Meter,   L"PaletteMeter",   L"PaletteGroupDisplay", L'\uE9D9',
                { PaletteArtShape::MeterBar, 7, 18, 1, 0.00, 0.80 } },
            { ControlKind::Lamp,    L"PaletteLamp",    L"PaletteGroupDisplay", L'\uEA80',
                { PaletteArtShape::Ellipse, 12, 12, 0, 0.00, 0.60 } },
            { ControlKind::Readout, L"PaletteReadout", L"PaletteGroupDisplay", L'\uE943',
                { PaletteArtShape::Sample, 26, 14, 0, 0.00, 0.00, L"0.00" } },
            { ControlKind::TimeDisplay, L"PaletteTimeDisplay", L"PaletteGroupDisplay", L'\uE916',
                { PaletteArtShape::Sample, 30, 14, 0, 0.00, 0.00, L"0:00" } },
            { ControlKind::Label,   L"PaletteLabel",   L"PaletteGroupDisplay", L'\uE8D2',
                { PaletteArtShape::Sample, 26, 14, 0, 0.00, 0.00, L"Aa" } },
            { ControlKind::Image,   L"PaletteImage",   L"PaletteGroupDisplay", L'\uEB9F',
                { PaletteArtShape::Glyph, 18, 18, 0, 0.00, 0.00, L"\uEB9F" } },

            // ---- Grouping ----
            { ControlKind::Panel,   L"PalettePanel",   L"PaletteGroupLayout", L'\uE7C1',
                { PaletteArtShape::Rectangle, 24, 16, 3, 0.70, 0.00 } },
        };

        return entries;
    }

    std::vector<std::wstring> PaletteGroups()
    {
        std::vector<std::wstring> groups{};

        for (auto const& entry : Palette())
        {
            if (std::find(groups.begin(), groups.end(), entry.GroupResourceKey) == groups.end())
            {
                groups.push_back(entry.GroupResourceKey);
            }
        }

        return groups;
    }

    _Use_decl_annotations_
    bool SendsAnything(ControlKind kind) noexcept
    {
        switch (kind)
        {
        case ControlKind::Meter:
        case ControlKind::Lamp:
        case ControlKind::Readout:
        case ControlKind::Label:
        case ControlKind::Image:
        case ControlKind::PageTab:
        case ControlKind::Panel:
        case ControlKind::TimeDisplay:
            return false;

        default:
            return true;
        }
    }

    _Use_decl_annotations_
    bool IsSquareByNature(ControlKind kind) noexcept
    {
        return kind == ControlKind::Knob ||
            kind == ControlKind::Encoder ||
            kind == ControlKind::Pad ||
            kind == ControlKind::XYPad ||
            kind == ControlKind::Joystick ||
            kind == ControlKind::Turntable;
    }

    _Use_decl_annotations_
    uint32_t NextFreeNumber(Page const& page, MessageKind kind, uint32_t first) noexcept
    {
        std::set<uint32_t> taken{};

        for (auto const& control : page.Controls)
        {
            for (auto const& message : control.Messages)
            {
                if (message.Kind == kind)
                {
                    taken.insert(message.Number);
                }
            }
        }

        auto candidate = first;

        while (taken.count(candidate) != 0 && candidate < 127)
        {
            ++candidate;
        }

        return candidate;
    }

    _Use_decl_annotations_
    Control MakeNewControl(
        ControlKind kind,
        double x,
        double y,
        int32_t pageWidth,
        int32_t pageHeight,
        std::wstring const& deviceName,
        Page const& page)
    {
        auto const size = DefaultControlSize(kind, pageWidth, pageHeight);

        Control control{};

        control.Id = LayoutDocument::NewId();
        control.Kind = kind;
        control.X = x;
        control.Y = y;
        control.Width = size.Width;
        control.Height = size.Height;
        control.HueSlot = NextHueSlot(page);
        control.KeyboardOrder = NextKeyboardOrder(page);
        control.AspectLocked = IsSquareByNature(kind);

        // A frame, not a filled box. A grouping panel that arrives as a solid plate competes
        // with the controls it is there to group; Plate and Solid are one click away in the
        // inspector for somebody who wants them.
        if (kind == ControlKind::Panel)
        {
            control.Style = ControlStyleOverride::Outline;
            control.LabelPlaced = LabelPlacementOverride::Inside;
        }

        // Text on the deck, with no plate and no rim. A caption that arrives inside a smoked
        // glass box is a caption somebody has to undo before they can use it; a border and a
        // background are one click away for anybody who wants them.
        if (kind == ControlKind::Label)
        {
            control.Style = ControlStyleOverride::Bare;
            control.LabelPlaced = LabelPlacementOverride::InsideCenter;
        }

        // A joystick that does not recenter is an XY pad drawn as a circle, so the spring is on
        // from the start and the middle is where it sits.
        if (kind == ControlKind::Joystick)
        {
            control.ReturnsToDefault = true;
            control.DefaultValue = 0.5;
            control.DefaultValueY = 0.5;
        }

        // Nothing rides a ribbon, so where it goes when the finger lifts is the first question
        // anybody asks about one. It holds its value until told otherwise.
        if (kind == ControlKind::Ribbon)
        {
            control.Ticks.Show = false;
        }

        if (kind == ControlKind::PianoKeyboard)
        {
            control.Ticks.Show = false;
            control.LabelPlaced = LabelPlacementOverride::None;
        }

        // A lamp sends nothing, so without a listener it is a dark circle that never does
        // anything, and somebody has to go and find the Listens tab to discover that. It
        // arrives watching the layout's device for any activity at all, which is what an
        // activity light is, and the rest of the panel narrows it from there.
        if (kind == ControlKind::Lamp)
        {
            control.Feedback.Enabled = true;
            control.Feedback.Mode = FeedbackMode::AnyActivity;
            control.Feedback.DeviceName = deviceName;
            control.Feedback.GroupIndex = AllGroups;
        }

        // A meter with nothing driving it never moves. One controller on the layout's device is
        // the ordinary case, and it is the row somebody would have had to fill in by hand.
        if (kind == ControlKind::Meter)
        {
            control.Feedback.Enabled = true;
            control.Feedback.Mode = FeedbackMode::Message;
            control.Feedback.Kind = MessageKind::ControlChange;
            control.Feedback.DeviceName = deviceName;
            control.Feedback.Number = NextFreeNumber(page, MessageKind::ControlChange, FirstController);
        }

        // A platter reports how far it has been pushed, not where it is, so it has to come back
        // to the middle the moment the hand leaves it. Without that, letting go of a nudge would
        // leave the deck running fast for the rest of the set.
        if (kind == ControlKind::Turntable)
        {
            control.ReturnsToDefault = true;
            control.DefaultValue = 0.5;
            control.LabelPlaced = LabelPlacementOverride::Below;
        }

        if (!SendsAnything(kind))
        {
            return control;
        }

        ControlMessage message{};

        message.DeviceName = deviceName;
        message.GroupIndex = 0;
        message.ChannelIndex = 0;

        switch (kind)
        {
        case ControlKind::Pad:
        case ControlKind::Button:
        case ControlKind::Toggle:
        {
            message.Trigger = MessageTrigger::TurnsOn;
            message.Kind = MessageKind::Note;
            message.Number = NextFreeNumber(page, MessageKind::Note, FirstNote);

            auto off = message;
            off.Trigger = MessageTrigger::TurnsOff;

            control.Messages.push_back(std::move(message));
            control.Messages.push_back(std::move(off));
            break;
        }

        // A keyboard plays whatever key was pressed, so the note number comes from the key
        // rather than from a row. One row says which device and channel, and that is all.
        case ControlKind::PianoKeyboard:
        {
            message.Trigger = MessageTrigger::Changes;
            message.Kind = MessageKind::Note;

            control.Messages.push_back(std::move(message));
            break;
        }

        // The clock generates its own stream. It carries a row only so it knows where to send.
        case ControlKind::BeatClock:
        {
            message.Trigger = MessageTrigger::Changes;
            message.Kind = MessageKind::RawUmp;

            control.Messages.push_back(std::move(message));
            break;
        }

        // Two axes, two rows, out of the box. A pad that only sends one is a pad somebody has
        // to finish wiring before it does what its name says.
        case ControlKind::XYPad:
        case ControlKind::Joystick:
        {
            message.Trigger = MessageTrigger::Changes;
            message.Kind = MessageKind::ControlChange;
            message.Number = NextFreeNumber(page, MessageKind::ControlChange, FirstController);
            message.Axis = ValueAxis::X;

            auto vertical = message;
            vertical.Axis = ValueAxis::Y;
            vertical.Number = message.Number + 1;

            control.Messages.push_back(std::move(message));
            control.Messages.push_back(std::move(vertical));
            break;
        }

        // Pitch bend, centered, because that is what a nudge is: push the platter and the deck
        // runs a little fast, let go and it settles back. A controller row set 0 to 127 gives
        // the other convention, where 64 means the wheel is not moving.
        case ControlKind::Turntable:
        {
            message.Trigger = MessageTrigger::Changes;
            message.Kind = MessageKind::PitchBend;

            control.Messages.push_back(std::move(message));
            break;
        }

        default:
        {
            message.Trigger = MessageTrigger::Changes;
            message.Kind = MessageKind::ControlChange;
            message.Number = NextFreeNumber(page, MessageKind::ControlChange, FirstController);

            control.Messages.push_back(std::move(message));
            break;
        }
        }

        return control;
    }
}
