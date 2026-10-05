// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML, so the unit tests compile it unchanged.

#include "ChannelVoiceWords.h"

#include <algorithm>
#include <cmath>

namespace midiapp
{
    namespace
    {
        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;

        constexpr uint8_t StatusRegisteredController = 0x2;
        constexpr uint8_t StatusAssignableController = 0x3;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        struct KindName
        {
            ValueMessageKind Kind;
            wchar_t const* Key;
        };

        constexpr KindName KindNames[]
        {
            { ValueMessageKind::ControlChange, L"controlChange" },
            { ValueMessageKind::PitchBend, L"pitchBend" },
            { ValueMessageKind::ChannelPressure, L"channelPressure" },
            { ValueMessageKind::PolyPressure, L"polyPressure" },
            { ValueMessageKind::RegisteredController, L"rpn" },
            { ValueMessageKind::AssignableController, L"nrpn" },
        };
    }

    _Use_decl_annotations_
    uint32_t ScaleToBits(double fraction, uint32_t bits) noexcept
    {
        if (!std::isfinite(fraction) || bits == 0 || bits > 32)
        {
            return 0;
        }

        auto const clamped = std::clamp(fraction, 0.0, 1.0);

        // The top of the range, not one short of it. A fader pushed all the way up has to send
        // 127, not 126, or every layout is quietly a little bit wrong at the top.
        auto const maximum = (bits >= 32)
            ? 4294967295.0
            : static_cast<double>((1u << bits) - 1u);

        return static_cast<uint32_t>(std::llround(clamped * maximum));
    }

    _Use_decl_annotations_
    uint32_t ClampToBits(double value, uint32_t bits) noexcept
    {
        if (!std::isfinite(value) || value <= 0.0 || bits == 0 || bits > 32)
        {
            return 0;
        }

        auto const maximum = (bits >= 32) ? 4294967295.0 : static_cast<double>((1u << bits) - 1u);

        return static_cast<uint32_t>(std::llround((std::min)(value, maximum)));
    }

    _Use_decl_annotations_
    uint32_t BuildMidi1ChannelVoice(
        uint8_t group,
        uint8_t status,
        uint8_t channel,
        uint8_t data1,
        uint8_t data2) noexcept
    {
        return (MessageTypeMidi1ChannelVoice << 28)
            | (static_cast<uint32_t>(group & 0x0F) << 24)
            | (static_cast<uint32_t>(status & 0x0F) << 20)
            | (static_cast<uint32_t>(channel & 0x0F) << 16)
            | (static_cast<uint32_t>(data1 & 0x7F) << 8)
            | static_cast<uint32_t>(data2 & 0x7F);
    }

    _Use_decl_annotations_
    void BuildMidi2ChannelVoice(
        uint8_t group,
        uint8_t status,
        uint8_t channel,
        uint8_t index1,
        uint8_t index2,
        uint32_t data,
        uint32_t* words) noexcept
    {
        words[0] = (MessageTypeMidi2ChannelVoice << 28)
            | (static_cast<uint32_t>(group & 0x0F) << 24)
            | (static_cast<uint32_t>(status & 0x0F) << 20)
            | (static_cast<uint32_t>(channel & 0x0F) << 16)
            | (static_cast<uint32_t>(index1) << 8)
            | static_cast<uint32_t>(index2);

        words[1] = data;
    }

    _Use_decl_annotations_
    uint32_t ValueMessageNumberMaximum(ValueMessageKind kind) noexcept
    {
        switch (kind)
        {
        case ValueMessageKind::ControlChange:
        case ValueMessageKind::PolyPressure:
            return 127;

        case ValueMessageKind::RegisteredController:
        case ValueMessageKind::AssignableController:
            return MaximumValueMessageNumber;

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    uint32_t BuildValueMessage(ValueMessageTarget const& target, double fraction, uint32_t* words) noexcept
    {
        auto const group = static_cast<uint8_t>(target.Group & 0x0F);
        auto const channel = static_cast<uint8_t>(target.Channel & 0x0F);
        auto const seven = static_cast<uint8_t>(target.Number & 0x7F);
        auto const midi1 = target.Midi1Protocol;

        switch (target.Kind)
        {
        case ValueMessageKind::PitchBend:
            if (midi1)
            {
                auto const bend = ScaleToBits(fraction, 14);

                words[0] = BuildMidi1ChannelVoice(group, StatusPitchBend, channel,
                    static_cast<uint8_t>(bend & 0x7F), static_cast<uint8_t>((bend >> 7) & 0x7F));
                return 1;
            }

            BuildMidi2ChannelVoice(group, StatusPitchBend, channel, 0, 0, ScaleToBits(fraction, 32), words);
            return 2;

        case ValueMessageKind::ChannelPressure:
            if (midi1)
            {
                words[0] = BuildMidi1ChannelVoice(group, StatusChannelPressure, channel,
                    static_cast<uint8_t>(ScaleToBits(fraction, 7)), 0);
                return 1;
            }

            BuildMidi2ChannelVoice(group, StatusChannelPressure, channel, 0, 0, ScaleToBits(fraction, 32), words);
            return 2;

        case ValueMessageKind::PolyPressure:
            if (midi1)
            {
                words[0] = BuildMidi1ChannelVoice(group, StatusPolyPressure, channel, seven,
                    static_cast<uint8_t>(ScaleToBits(fraction, 7)));
                return 1;
            }

            BuildMidi2ChannelVoice(group, StatusPolyPressure, channel, seven, 0, ScaleToBits(fraction, 32), words);
            return 2;

        case ValueMessageKind::RegisteredController:
        case ValueMessageKind::AssignableController:
        {
            auto const number = (std::min)(target.Number, MaximumValueMessageNumber);

            BuildMidi2ChannelVoice(group,
                target.Kind == ValueMessageKind::RegisteredController ? StatusRegisteredController : StatusAssignableController,
                channel,
                static_cast<uint8_t>((number >> 7) & 0x7F),
                static_cast<uint8_t>(number & 0x7F),
                ScaleToBits(fraction, 32),
                words);
            return 2;
        }

        case ValueMessageKind::ControlChange:
        default:
            if (midi1)
            {
                words[0] = BuildMidi1ChannelVoice(group, StatusControlChange, channel, seven,
                    static_cast<uint8_t>(ScaleToBits(fraction, 7)));
                return 1;
            }

            BuildMidi2ChannelVoice(group, StatusControlChange, channel, seven, 0, ScaleToBits(fraction, 32), words);
            return 2;
        }
    }

    _Use_decl_annotations_
    std::wstring_view ValueMessageKindKey(ValueMessageKind kind) noexcept
    {
        for (auto const& entry : KindNames)
        {
            if (entry.Kind == kind)
            {
                return entry.Key;
            }
        }

        return KindNames[0].Key;
    }

    _Use_decl_annotations_
    std::optional<ValueMessageKind> ValueMessageKindFromKey(std::wstring_view key) noexcept
    {
        for (auto const& entry : KindNames)
        {
            if (key == entry.Key)
            {
                return entry.Kind;
            }
        }

        return std::nullopt;
    }
}
