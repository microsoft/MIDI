// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "LayoutTemplates.h"
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
            _In_ std::wstring pageName) noexcept
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
    }

    std::vector<LayoutTemplateInfo> const& LayoutTemplates() noexcept
    {
        static std::vector<LayoutTemplateInfo> const templates
        {
            { LayoutTemplateKind::Mixer,     L"TemplateMixerName",     L"TemplateMixerDescription",     L'\xE9F5' },
            { LayoutTemplateKind::DjDeck,    L"TemplateDjDeckName",    L"TemplateDjDeckDescription",    L'\xE93C' },
            { LayoutTemplateKind::DrumPads,  L"TemplateDrumPadsName",  L"TemplateDrumPadsDescription",  L'\xE80A' },
            { LayoutTemplateKind::Transport, L"TemplateTransportName", L"TemplateTransportDescription", L'\xE768' },
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
        switch (kind)
        {
        case LayoutTemplateKind::Mixer:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Mixer");
            FillMixer(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::DjDeck:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Decks");
            FillDjDeck(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::DrumPads:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Pads");
            FillDrumPads(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::Transport:
        {
            auto document = MakeDocument(layoutName, deviceName, match, matchMode, L"Transport");
            FillTransport(document, deviceName);
            return document;
        }

        case LayoutTemplateKind::Blank:
        default:
            return MakeDocument(layoutName, deviceName, match, matchMode, L"Page 1");
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
