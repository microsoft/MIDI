// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "StarterLayout.h"
#include "PageTemplates.h"

#include <format>

namespace glass
{
    namespace
    {
        constexpr int32_t PageWidth = 1280;
        constexpr int32_t PageHeight = 800;

        constexpr int32_t ColumnGap = 24;

        constexpr int32_t FaderRowTop = 96;
        constexpr int32_t KnobRowTop = 440;
        constexpr int32_t PadRowTop = 600;

        Control MakeControl(
            _In_ ControlKind kind,
            _In_ std::wstring label,
            _In_ double x,
            _In_ double y,
            _In_ ControlSize const& size,
            _In_ int32_t hueSlot,
            _In_ int32_t keyboardOrder) noexcept
        {
            Control control{};

            control.Id = LayoutDocument::NewId();
            control.Kind = kind;
            control.Label = std::move(label);
            control.X = x;
            control.Y = y;
            control.Width = size.Width;
            control.Height = size.Height;
            control.HueSlot = hueSlot;
            control.KeyboardOrder = keyboardOrder;

            return control;
        }

        ControlMessage MakeMessage(
            _In_ MessageTrigger trigger,
            _In_ MessageKind kind,
            _In_ std::wstring const& deviceName,
            _In_ uint32_t number) noexcept
        {
            ControlMessage message{};

            message.Trigger = trigger;
            message.Kind = kind;
            message.DeviceName = deviceName;
            message.GroupIndex = 0;
            message.ChannelIndex = 0;
            message.Number = number;

            return message;
        }
    }

    _Use_decl_annotations_
    LayoutDocument BuildStarterLayout(
        std::wstring const& layoutName,
        std::wstring const& deviceName,
        midiapp::EndpointMatch const& match,
        midiapp::EndpointMatchMode matchMode) noexcept
    {
        LayoutDocument document{};

        document.Name = layoutName;
        document.PageWidth = PageWidth;
        document.PageHeight = PageHeight;
        document.CanvasWidth = PageWidth;
        document.CanvasHeight = PageHeight;
        document.ThemeName = L"Studio Dark";
        document.Scale = ScaleMode::FitToScreen;

        DeviceEntry device{};
        device.Name = deviceName;
        device.Match = match;
        device.MatchMode = matchMode;

        document.Devices.push_back(device);

        Page page{};
        page.Id = LayoutDocument::NewId();
        page.Name = L"Mixer";

        auto const faderSize = DefaultControlSize(ControlKind::Fader, PageWidth, PageHeight);
        auto const knobSize = DefaultControlSize(ControlKind::Knob, PageWidth, PageHeight);
        auto const padSize = DefaultControlSize(ControlKind::Pad, PageWidth, PageHeight);

        int32_t order{ 0 };

        auto const spread = [](int32_t count, int32_t itemWidth) noexcept
            {
                auto const total = count * itemWidth + (count - 1) * ColumnGap;
                return (PageWidth - total) / 2;
            };

        auto const faderLeft = spread(static_cast<int32_t>(StarterFaderCount), faderSize.Width);

        for (uint32_t i = 0; i < StarterFaderCount; ++i)
        {
            auto control = MakeControl(
                ControlKind::Fader,
                std::format(L"CC {}", StarterFirstFaderController + i),
                faderLeft + static_cast<int32_t>(i) * (faderSize.Width + ColumnGap),
                FaderRowTop,
                faderSize,
                static_cast<int32_t>(i % HueSlotCount),
                order++);

            control.Messages.push_back(MakeMessage(
                MessageTrigger::Changes,
                MessageKind::ControlChange,
                deviceName,
                StarterFirstFaderController + i));

            page.Controls.push_back(std::move(control));
        }

        auto const knobLeft = spread(static_cast<int32_t>(StarterKnobCount), knobSize.Width);

        for (uint32_t i = 0; i < StarterKnobCount; ++i)
        {
            auto control = MakeControl(
                ControlKind::Knob,
                std::format(L"CC {}", StarterFirstKnobController + i),
                knobLeft + static_cast<int32_t>(i) * (knobSize.Width + ColumnGap),
                KnobRowTop,
                knobSize,
                static_cast<int32_t>((i + 2) % HueSlotCount),
                order++);

            control.Messages.push_back(MakeMessage(
                MessageTrigger::Changes,
                MessageKind::ControlChange,
                deviceName,
                StarterFirstKnobController + i));

            page.Controls.push_back(std::move(control));
        }

        auto const padLeft = spread(static_cast<int32_t>(StarterPadCount), padSize.Width);

        for (uint32_t i = 0; i < StarterPadCount; ++i)
        {
            auto control = MakeControl(
                ControlKind::Pad,
                std::format(L"Note {}", StarterFirstPadNote + i),
                padLeft + static_cast<int32_t>(i) * (padSize.Width + ColumnGap),
                PadRowTop,
                padSize,
                static_cast<int32_t>((i + 4) % HueSlotCount),
                order++);

            // A pad is one note, and it sits at one end of its range or the other, which is why
            // there is no separate on and off pair: off is the minimum and on is the maximum.
            auto on = MakeMessage(
                MessageTrigger::TurnsOn,
                MessageKind::Note,
                deviceName,
                StarterFirstPadNote + i);

            on.Minimum = { 0.0, ValueScaling::Fraction };
            on.Maximum = { 1.0, ValueScaling::Fraction };

            auto off = on;
            off.Trigger = MessageTrigger::TurnsOff;

            control.Messages.push_back(std::move(on));
            control.Messages.push_back(std::move(off));

            page.Controls.push_back(std::move(control));
        }

        document.Pages.push_back(std::move(page));

        return document;
    }
}
