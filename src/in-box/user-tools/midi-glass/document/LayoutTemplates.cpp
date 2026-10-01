// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "LayoutTemplates.h"
#include "MackieControl.h"
#include "PageTemplates.h"

#include <format>

namespace glass
{
    namespace
    {
        constexpr int32_t PageWidth = 1280;
        constexpr int32_t PageHeight = 800;

        constexpr int32_t ColumnGap = 24;

        // Channel 10 in the way people say it, which is index 9.
        constexpr int32_t DrumChannelIndex = 9;

        int32_t CenteredLeft(_In_ int32_t count, _In_ int32_t itemWidth, _In_ int32_t gap) noexcept
        {
            auto const total = count * itemWidth + (count - 1) * gap;
            return (PageWidth - total) / 2;
        }

        Control MakeControl(
            _In_ ControlKind kind,
            _In_ std::wstring label,
            _In_ double x,
            _In_ double y,
            _In_ double width,
            _In_ double height,
            _In_ int32_t hueSlot,
            _In_ int32_t keyboardOrder) noexcept
        {
            Control control{};

            control.Id = LayoutDocument::NewId();
            control.Kind = kind;
            control.Label = std::move(label);
            control.X = x;
            control.Y = y;
            control.Width = width;
            control.Height = height;
            control.HueSlot = hueSlot;
            control.KeyboardOrder = keyboardOrder;

            return control;
        }

        ControlMessage MakeMessage(
            _In_ MessageTrigger trigger,
            _In_ MessageKind kind,
            _In_ std::wstring const& deviceName,
            _In_ uint32_t number,
            _In_ int32_t channelIndex = 0) noexcept
        {
            ControlMessage message{};

            message.Trigger = trigger;
            message.Kind = kind;
            message.DeviceName = deviceName;
            message.GroupIndex = 0;
            message.ChannelIndex = channelIndex;
            message.Number = number;

            return message;
        }

        void AddNotePair(
            _Inout_ Control& control,
            _In_ std::wstring const& deviceName,
            _In_ uint32_t note,
            _In_ int32_t channelIndex) noexcept
        {
            // A pad sits at one end of its range or the other, which is why there is no separate
            // on and off pair: off is the minimum and on is the maximum.
            auto on = MakeMessage(MessageTrigger::TurnsOn, MessageKind::Note, deviceName, note, channelIndex);

            auto off = on;
            off.Trigger = MessageTrigger::TurnsOff;

            control.Messages.push_back(std::move(on));
            control.Messages.push_back(std::move(off));
        }

        LayoutDocument MakeDocument(
            _In_ std::wstring const& layoutName,
            _In_ std::wstring const& deviceName,
            _In_ midiapp::EndpointMatch const& match,
            _In_ midiapp::EndpointMatchMode matchMode,
            _In_ std::wstring pageName,
            _In_ std::wstring themeName) noexcept
        {
            LayoutDocument document{};

            document.Name = layoutName;
            document.PageWidth = PageWidth;
            document.PageHeight = PageHeight;
            document.CanvasWidth = PageWidth;
            document.CanvasHeight = PageHeight;
            document.ThemeName = std::move(themeName);
            document.Scale = ScaleMode::FitToScreen;

            DeviceEntry device{};
            device.Name = deviceName;
            device.Match = match;
            device.MatchMode = matchMode;

            document.Devices.push_back(device);

            Page page{};
            page.Id = LayoutDocument::NewId();
            page.Name = std::move(pageName);

            document.Pages.push_back(std::move(page));

            return document;
        }

        // ------------------------------------------------------------------ mixer

        void FillMixer(_Inout_ LayoutDocument& document, _In_ std::wstring const& deviceName) noexcept
        {
            auto& page = document.Pages[0];

            auto const faderSize = DefaultControlSize(ControlKind::Fader, PageWidth, PageHeight);
            auto const knobSize = DefaultControlSize(ControlKind::Knob, PageWidth, PageHeight);
            auto const padSize = DefaultControlSize(ControlKind::Pad, PageWidth, PageHeight);

            int32_t order{ 0 };

            auto const faderLeft = CenteredLeft(
                static_cast<int32_t>(StarterFaderCount), faderSize.Width, ColumnGap);

            for (uint32_t i = 0; i < StarterFaderCount; ++i)
            {
                auto control = MakeControl(
                    ControlKind::Fader,
                    std::format(L"CC {}", StarterFirstFaderController + i),
                    faderLeft + static_cast<int32_t>(i) * (faderSize.Width + ColumnGap),
                    96,
                    faderSize.Width,
                    faderSize.Height,
                    static_cast<int32_t>(i % HueSlotCount),
                    order++);

                control.Messages.push_back(MakeMessage(
                    MessageTrigger::Changes,
                    MessageKind::ControlChange,
                    deviceName,
                    StarterFirstFaderController + i));

                page.Controls.push_back(std::move(control));
            }

            auto const knobLeft = CenteredLeft(
                static_cast<int32_t>(StarterKnobCount), knobSize.Width, ColumnGap);

            for (uint32_t i = 0; i < StarterKnobCount; ++i)
            {
                auto control = MakeControl(
                    ControlKind::Knob,
                    std::format(L"CC {}", StarterFirstKnobController + i),
                    knobLeft + static_cast<int32_t>(i) * (knobSize.Width + ColumnGap),
                    440,
                    knobSize.Width,
                    knobSize.Height,
                    static_cast<int32_t>((i + 2) % HueSlotCount),
                    order++);

                control.Messages.push_back(MakeMessage(
                    MessageTrigger::Changes,
                    MessageKind::ControlChange,
                    deviceName,
                    StarterFirstKnobController + i));

                page.Controls.push_back(std::move(control));
            }

            auto const padLeft = CenteredLeft(
                static_cast<int32_t>(StarterPadCount), padSize.Width, ColumnGap);

            for (uint32_t i = 0; i < StarterPadCount; ++i)
            {
                auto control = MakeControl(
                    ControlKind::Pad,
                    std::format(L"Note {}", StarterFirstPadNote + i),
                    padLeft + static_cast<int32_t>(i) * (padSize.Width + ColumnGap),
                    600,
                    padSize.Width,
                    padSize.Height,
                    static_cast<int32_t>((i + 4) % HueSlotCount),
                    order++);

                AddNotePair(control, deviceName, StarterFirstPadNote + i, 0);

                page.Controls.push_back(std::move(control));
            }
        }

        // ---------------------------------------------------------------- DJ deck

        void FillDjDeck(_Inout_ LayoutDocument& document, _In_ std::wstring const& deviceName) noexcept
        {
            auto& page = document.Pages[0];

            int32_t order{ 0 };

            // Two decks, laid out as a mirror pair with the crossfader between them.
            for (int32_t deck = 0; deck < 2; ++deck)
            {
                auto const left = deck == 0 ? 120 : 800;
                auto const channel = deck;
                auto const hue = deck == 0 ? 0 : 5;

                auto filter = MakeControl(
                    ControlKind::Knob,
                    deck == 0 ? L"Filter A" : L"Filter B",
                    left + 96, 100, 128, 128, hue, order++);

                filter.DefaultValue = 0.5;
                filter.Messages.push_back(MakeMessage(
                    MessageTrigger::Changes, MessageKind::ControlChange, deviceName, 74, channel));

                page.Controls.push_back(std::move(filter));

                wchar_t const* const bandNames[]{ L"Low", L"Mid", L"High" };

                for (int32_t band = 0; band < 3; ++band)
                {
                    auto eq = MakeControl(
                        ControlKind::Fader,
                        bandNames[band],
                        left + band * 88, 264, 56, 240, hue, order++);

                    eq.DefaultValue = 0.5;
                    eq.Messages.push_back(MakeMessage(
                        MessageTrigger::Changes, MessageKind::ControlChange, deviceName,
                        static_cast<uint32_t>(20 + band), channel));

                    page.Controls.push_back(std::move(eq));
                }

                for (int32_t cue = 0; cue < 4; ++cue)
                {
                    auto pad = MakeControl(
                        ControlKind::Pad,
                        std::format(L"Cue {}", cue + 1),
                        left + cue * 72, 552, 64, 64,
                        (hue + 2 + cue) % HueSlotCount, order++);

                    AddNotePair(pad, deviceName, static_cast<uint32_t>(48 + cue), channel);

                    page.Controls.push_back(std::move(pad));
                }
            }

            auto crossfader = MakeControl(ControlKind::Fader, L"Crossfader", 424, 660, 432, 64, 3, order++);

            crossfader.DefaultValue = 0.5;
            crossfader.Messages.push_back(MakeMessage(
                MessageTrigger::Changes, MessageKind::ControlChange, deviceName, 8));

            page.Controls.push_back(std::move(crossfader));

            auto master = MakeControl(ControlKind::Fader, L"Master", 600, 168, 80, 400, 2, order++);

            master.DefaultValue = 0.8;
            master.Messages.push_back(MakeMessage(
                MessageTrigger::Changes, MessageKind::ControlChange, deviceName, 7));

            page.Controls.push_back(std::move(master));
        }

        // -------------------------------------------------------------- drum pads

        void FillDrumPads(_Inout_ LayoutDocument& document, _In_ std::wstring const& deviceName) noexcept
        {
            auto& page = document.Pages[0];

            constexpr int32_t padSize = 128;
            constexpr int32_t gap = 16;
            constexpr int32_t columns = 4;
            constexpr int32_t rows = 4;

            auto const left = CenteredLeft(columns, padSize, gap);
            constexpr int32_t top = 120;

            int32_t order{ 0 };

            // Bottom left is the lowest note, the way every pad grid is laid out, so the kick
            // ends up under the hand that expects it.
            for (int32_t row = 0; row < rows; ++row)
            {
                for (int32_t column = 0; column < columns; ++column)
                {
                    auto const note = static_cast<uint32_t>(
                        StarterFirstPadNote + (rows - 1 - row) * columns + column);

                    auto pad = MakeControl(
                        ControlKind::Pad,
                        std::format(L"{}", note),
                        left + column * (padSize + gap),
                        top + row * (padSize + gap),
                        padSize,
                        padSize,
                        (row + column) % HueSlotCount,
                        order++);

                    AddNotePair(pad, deviceName, note, DrumChannelIndex);

                    page.Controls.push_back(std::move(pad));
                }
            }
        }

        // -------------------------------------------------------------- transport

        void FillTransport(_Inout_ LayoutDocument& document, _In_ std::wstring const& deviceName) noexcept
        {
            auto& page = document.Pages[0];

            int32_t order{ 0 };

            // Mackie Control note numbers, which is what most DAWs already listen for.
            struct TransportButton
            {
                wchar_t const* Label;
                uint32_t Note;
                int32_t HueSlot;
                ControlKind Kind;
            };

            constexpr TransportButton buttons[]
            {
                { L"Rewind",  91, 0, ControlKind::Button },
                { L"Forward", 92, 0, ControlKind::Button },
                { L"Stop",    93, 1, ControlKind::Button },
                { L"Play",    94, 1, ControlKind::Button },
                { L"Record",  95, 5, ControlKind::Button },
                { L"Loop",    86, 4, ControlKind::Toggle },
            };

            constexpr int32_t buttonWidth = 136;
            constexpr int32_t buttonHeight = 96;
            constexpr int32_t gap = 20;

            auto const left = CenteredLeft(
                static_cast<int32_t>(std::size(buttons)), buttonWidth, gap);

            for (size_t i = 0; i < std::size(buttons); ++i)
            {
                auto const& definition = buttons[i];

                auto control = MakeControl(
                    definition.Kind,
                    definition.Label,
                    left + static_cast<int32_t>(i) * (buttonWidth + gap),
                    160,
                    buttonWidth,
                    buttonHeight,
                    definition.HueSlot,
                    order++);

                AddNotePair(control, deviceName, definition.Note, 0);

                page.Controls.push_back(std::move(control));
            }

            auto const faderSize = DefaultControlSize(ControlKind::Fader, PageWidth, PageHeight);
            auto const faderLeft = CenteredLeft(8, faderSize.Width, ColumnGap);

            for (uint32_t i = 0; i < 8; ++i)
            {
                auto control = MakeControl(
                    ControlKind::Fader,
                    std::format(L"Track {}", i + 1),
                    faderLeft + static_cast<int32_t>(i) * (faderSize.Width + ColumnGap),
                    344,
                    faderSize.Width,
                    faderSize.Height,
                    static_cast<int32_t>(i % HueSlotCount),
                    order++);

                control.DefaultValue = 0.78;
                control.Messages.push_back(MakeMessage(
                    MessageTrigger::Changes, MessageKind::ControlChange, deviceName, 7, static_cast<int32_t>(i)));

                page.Controls.push_back(std::move(control));
            }
        }

        // ------------------------------------------------ floating toolbars

        // The window is the page, only the buttons are drawn, and it stays in front of the app
        // it sits over.
        void MakeFloating(_Inout_ LayoutDocument& document, _In_ int32_t width, _In_ int32_t height) noexcept
        {
            document.PageWidth = width;
            document.PageHeight = height;
            document.CanvasWidth = width;
            document.CanvasHeight = height;
            document.Scale = ScaleMode::ActualSize;
            document.ToolbarWindow = true;
            document.SeeThrough = true;
            document.AlwaysOnTop = true;
        }

        // Square buttons, evenly spaced and centered, each playing the next note up.
        void FillButtons(
            _Inout_ LayoutDocument& document,
            _In_ std::wstring const& deviceName,
            _In_ int32_t columns,
            _In_ int32_t rows,
            _In_ int32_t side) noexcept
        {
            constexpr int32_t gap = 16;

            auto& page = document.Pages[0];

            auto const left = (document.PageWidth - (columns * side + (columns - 1) * gap)) / 2;
            auto const top = (document.PageHeight - (rows * side + (rows - 1) * gap)) / 2;

            int32_t order{ 0 };

            for (int32_t row = 0; row < rows; ++row)
            {
                for (int32_t column = 0; column < columns; ++column)
                {
                    auto button = MakeControl(
                        ControlKind::Button,
                        std::format(L"{}", order + 1),
                        left + column * (side + gap),
                        top + row * (side + gap),
                        side,
                        side,
                        order % HueSlotCount,
                        order);

                    AddNotePair(button, deviceName, StarterFirstPadNote + static_cast<uint32_t>(order), 0);

                    page.Controls.push_back(std::move(button));
                    ++order;
                }
            }
        }

        // --------------------------------------------------------- Mackie Control

        // The DAW decides what every one of these does and lights them, so each control is a
        // function rather than a message, and every button is momentary.
        void FillMackieControl(_Inout_ LayoutDocument& document, _In_ std::wstring const& deviceName) noexcept
        {
            constexpr int32_t stripWidth = 112;
            constexpr int32_t stripLeft = 24;
            constexpr int32_t buttonWidth = 88;
            constexpr int32_t buttonHeight = 40;
            constexpr int32_t knobSide = 72;
            constexpr int32_t faderWidth = 64;
            constexpr int32_t faderTop = 312;
            constexpr int32_t faderHeight = 440;

            document.Devices[0].Protocol = DeviceProtocol::MackieControl;

            auto& page = document.Pages[0];

            int32_t order{ 0 };

            auto const add = [&](ControlKind kind, std::wstring label, int32_t x, int32_t y, int32_t width, int32_t height, int32_t hue, uint32_t function)
                {
                    auto control = MakeControl(kind, std::move(label), x, y, width, height, hue, order++);

                    control.Messages.push_back(MakeMackieRow(function, deviceName, 0));

                    // A turn is sent from how far the control moved, so it comes back to the middle.
                    if (ShapeOfMackieFunction(function) == MackieShape::Encoder)
                    {
                        control.ReturnsToDefault = true;
                        control.DefaultValue = 0.5;
                    }

                    page.Controls.push_back(std::move(control));
                };

            struct StripButton
            {
                wchar_t const* Label;
                uint32_t FirstNote;
                int32_t HueSlot;
            };

            constexpr StripButton stripButtons[]
            {
                { L"Rec", 0, 5 },
                { L"Solo", 8, 3 },
                { L"Mute", 16, 2 },
                { L"Select", 24, 0 },
            };

            for (uint32_t strip = 0; strip < MackieStripCount; ++strip)
            {
                auto const left = stripLeft + static_cast<int32_t>(strip) * stripWidth;
                auto const number = strip + 1;

                add(ControlKind::Knob, std::format(L"V-Pot {}", number),
                    left + (stripWidth - knobSide) / 2, 24, knobSide, knobSide, 4, MackieVPotBase + strip);

                for (size_t row = 0; row < std::size(stripButtons); ++row)
                {
                    auto const& button = stripButtons[row];

                    add(ControlKind::Button, std::format(L"{} {}", button.Label, number),
                        left + (stripWidth - buttonWidth) / 2, 112 + static_cast<int32_t>(row) * 48,
                        buttonWidth, buttonHeight, button.HueSlot, button.FirstNote + strip);
                }

                add(ControlKind::Fader, std::format(L"Fader {}", number),
                    left + (stripWidth - faderWidth) / 2, faderTop, faderWidth, faderHeight, 4, MackieFaderBase + strip);
            }

            add(ControlKind::Fader, L"Master",
                stripLeft + static_cast<int32_t>(MackieStripCount) * stripWidth + (stripWidth - faderWidth) / 2,
                faderTop, faderWidth, faderHeight, 1, MackieFaderBase + MackieMasterStrip);

            // Transport, banks, cursor keys and the jog wheel down the right.
            constexpr int32_t panelLeft = 1060;
            constexpr int32_t smallWidth = 60;
            constexpr int32_t smallHeight = 48;
            constexpr int32_t smallStep = 68;

            struct PanelButton
            {
                wchar_t const* Label;
                uint32_t Note;
                int32_t Column;
                int32_t Top;
                int32_t HueSlot;
            };

            constexpr PanelButton transport[]
            {
                { L"Rewind", 91, 0, 24, 0 },
                { L"Forward", 92, 1, 24, 0 },
                { L"Stop", 93, 2, 24, 1 },
                { L"Play", 94, 0, 80, 1 },
                { L"Record", 95, 1, 80, 5 },
                { L"Cycle", 86, 2, 80, 4 },
                { L"Up", 96, 1, 504, 2 },
                { L"Left", 98, 0, 560, 2 },
                { L"Zoom", 100, 1, 560, 2 },
                { L"Right", 99, 2, 560, 2 },
                { L"Down", 97, 1, 616, 2 },
            };

            for (auto const& button : transport)
            {
                add(ControlKind::Button, button.Label, panelLeft + button.Column * smallStep, button.Top,
                    smallWidth, smallHeight, button.HueSlot, button.Note);
            }

            constexpr PanelButton banks[]
            {
                { L"Bank left", 46, 0, 152, 3 },
                { L"Bank right", 47, 1, 152, 3 },
                { L"Channel left", 48, 0, 200, 3 },
                { L"Channel right", 49, 1, 200, 3 },
            };

            for (auto const& button : banks)
            {
                add(ControlKind::Button, button.Label, panelLeft + button.Column * 100, button.Top,
                    96, buttonHeight, button.HueSlot, button.Note);
            }

            add(ControlKind::Turntable, L"Jog", panelLeft, 264, 196, 196, 3, MackieJog);
        }
    }

    std::vector<LayoutTemplateInfo> const& LayoutTemplates() noexcept
    {
        static std::vector<LayoutTemplateInfo> const templates
        {
            { LayoutTemplateKind::Mixer,     L"TemplateMixerName",     L"TemplateMixerDescription",     L'\xE9F5' },
            { LayoutTemplateKind::DjDeck,    L"TemplateDjDeckName",    L"TemplateDjDeckDescription",    L'\xE93C' },
            { LayoutTemplateKind::DrumPads,  L"TemplateDrumPadsName",  L"TemplateDrumPadsDescription",  L'\xE80A' },
            { LayoutTemplateKind::Transport, L"TemplateTransportName", L"TemplateTransportDescription", L'\xE768' },
            { LayoutTemplateKind::MackieControl, L"TemplateMackieControlName", L"TemplateMackieControlDescription", L'\xE9E9' },
            { LayoutTemplateKind::HorizontalToolbar, L"TemplateHorizontalToolbarName", L"TemplateHorizontalToolbarDescription", L'\xE90E' },
            { LayoutTemplateKind::VerticalToolbar, L"TemplateVerticalToolbarName", L"TemplateVerticalToolbarDescription", L'\xE90C' },
            { LayoutTemplateKind::FloatingPalette, L"TemplateFloatingPaletteName", L"TemplateFloatingPaletteDescription", L'\xE790' },
            { LayoutTemplateKind::Blank,     L"TemplateBlankName",     L"TemplateBlankDescription",     L'\xE7C3' },
        };

        return templates;
    }

    _Use_decl_annotations_
    LayoutDocument BuildLayoutFromTemplate(
        LayoutTemplateKind kind,
        std::wstring const& layoutName,
        std::wstring const& deviceName,
        midiapp::EndpointMatch const& match,
        midiapp::EndpointMatchMode matchMode) noexcept
    {
        // A shipped theme per starter: the nearly black default made a new page look empty.
        switch (kind)
        {
        case LayoutTemplateKind::Mixer:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Mixer", L"Bigwig");
            FillMixer(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::DjDeck:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Decks", L"Supersaw");
            FillDjDeck(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::DrumPads:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Pads", L"Insert Coin");
            FillDrumPads(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::Transport:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Transport", L"Daylight");
            FillTransport(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::HorizontalToolbar:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Toolbar", L"Bone");
            MakeFloating(document, ToolbarLength, ToolbarThickness);
            FillButtons(document, deviceName, 8, 1, 80);
            return document;
        }

        case LayoutTemplateKind::VerticalToolbar:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Toolbar", L"Bone");
            MakeFloating(document, ToolbarThickness, ToolbarLength);
            FillButtons(document, deviceName, 1, 8, 80);
            return document;
        }

        case LayoutTemplateKind::FloatingPalette:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Palette", L"Bone");
            MakeFloating(document, PaletteSide, PaletteSide);
            FillButtons(document, deviceName, 4, 4, 72);
            return document;
        }

        case LayoutTemplateKind::MackieControl:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Mixer", L"Bigwig");
            FillMackieControl(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::Blank:
        default:
            return MakeDocument(layoutName, deviceName, match, matchMode, L"Page 1", L"Tonal Light");
        }
    }

    _Use_decl_annotations_
    LayoutDocument BuildStarterLayout(
        std::wstring const& layoutName,
        std::wstring const& deviceName,
        midiapp::EndpointMatch const& match,
        midiapp::EndpointMatchMode matchMode) noexcept
    {
        return BuildLayoutFromTemplate(
            LayoutTemplateKind::Mixer, layoutName, deviceName, match, matchMode);
    }
}
