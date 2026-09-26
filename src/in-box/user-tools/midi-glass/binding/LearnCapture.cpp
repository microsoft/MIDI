// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK, so the unit tests compile it unchanged.

#include "LearnCapture.h"

namespace glass
{
    namespace
    {
        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;

        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusPolyPressure = 0xA;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        constexpr uint8_t StatusRegisteredController = 0x2;
        constexpr uint8_t StatusAssignedController = 0x3;
        constexpr uint8_t StatusPerNoteController = 0x0;

        constexpr double Normalize(_In_ uint32_t value, _In_ uint32_t bits) noexcept
        {
            if (bits == 0 || bits > 32)
            {
                return 0.0;
            }

            auto const top = bits == 32
                ? 4294967295.0
                : static_cast<double>((1u << bits) - 1);

            return static_cast<double>(value) / top;
        }
    }

    _Use_decl_annotations_
    bool TryLearnFromWords(uint32_t const* words, uint32_t wordCount, LearnedBinding& learned) noexcept
    {
        learned = {};

        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        auto const messageType = (words[0] >> 28) & 0xF;
        auto const group = static_cast<int32_t>((words[0] >> 24) & 0xF);
        auto const status = static_cast<uint8_t>((words[0] >> 20) & 0xF);
        auto const channel = static_cast<int32_t>((words[0] >> 16) & 0xF);

        learned.GroupIndex = group;
        learned.ChannelIndex = channel;

        if (messageType == MessageTypeMidi1ChannelVoice)
        {
            auto const data1 = static_cast<uint32_t>((words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint32_t>(words[0] & 0x7F);

            switch (status)
            {
            case StatusNoteOn:
            case StatusNoteOff:
                learned.Kind = MessageKind::Note;
                learned.Number = data1;
                learned.Value = status == StatusNoteOn ? Normalize(data2, 7) : 0.0;
                return true;

            case StatusControlChange:
                learned.Kind = MessageKind::ControlChange;
                learned.Number = data1;
                learned.Value = Normalize(data2, 7);
                return true;

            case StatusProgramChange:
                learned.Kind = MessageKind::ProgramChange;
                learned.Number = data1;
                learned.Value = 1.0;
                return true;

            case StatusChannelPressure:
                learned.Kind = MessageKind::ChannelPressure;
                learned.Value = Normalize(data1, 7);
                return true;

            case StatusPitchBend:
                learned.Kind = MessageKind::PitchBend;
                learned.Value = Normalize((data2 << 7) | data1, 14);
                return true;

            case StatusPolyPressure:
                learned.Kind = MessageKind::PerNoteController;
                learned.Number = data1;
                learned.Value = Normalize(data2, 7);
                return true;

            default:
                return false;
            }
        }

        if (messageType != MessageTypeMidi2ChannelVoice || wordCount < 2)
        {
            // Clock, active sensing, stream messages and system exclusive all arrive constantly
            // on a busy port. None of them is something a control can be bound to.
            return false;
        }

        auto const index1 = static_cast<uint32_t>((words[0] >> 8) & 0x7F);
        auto const index2 = static_cast<uint32_t>(words[0] & 0x7F);

        switch (status)
        {
        case StatusNoteOn:
        case StatusNoteOff:
            learned.Kind = MessageKind::Note;
            learned.Number = index1;
            learned.Value = status == StatusNoteOn ? Normalize(words[1] >> 16, 16) : 0.0;
            return true;

        case StatusControlChange:
            learned.Kind = MessageKind::ControlChange;
            learned.Number = index1;
            learned.Value = Normalize(words[1], 32);
            return true;

        case StatusProgramChange:
            learned.Kind = MessageKind::ProgramChange;
            learned.Number = (words[1] >> 24) & 0x7F;
            learned.Value = 1.0;
            return true;

        case StatusChannelPressure:
            learned.Kind = MessageKind::ChannelPressure;
            learned.Value = Normalize(words[1], 32);
            return true;

        case StatusPitchBend:
            learned.Kind = MessageKind::PitchBend;
            learned.Value = Normalize(words[1], 32);
            return true;

        case StatusRegisteredController:
        case StatusAssignedController:
            learned.Kind = status == StatusRegisteredController
                ? MessageKind::RegisteredController
                : MessageKind::AssignedController;
            learned.Number = (index1 << 7) | index2;
            learned.Value = Normalize(words[1], 32);
            return true;

        case StatusPerNoteController:
            learned.Kind = MessageKind::PerNoteController;
            learned.Number = index1;
            learned.Value = Normalize(words[1], 32);
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool IsWorthLearning(LearnedBinding const& learned) noexcept
    {
        // A note off, a control change at zero and a pitch bend sitting at center all arrive
        // while somebody is still reaching for the knob they mean. Taking the first message that
        // turns up would bind the wrong thing, and they would have to start again.
        if (learned.Kind == MessageKind::PitchBend)
        {
            return learned.Value < 0.45 || learned.Value > 0.55;
        }

        return learned.Value > 0.0;
    }

    _Use_decl_annotations_
    void ApplyLearned(
        LearnedBinding const& learned,
        LearnAcceptance const& accept,
        ControlMessage& message) noexcept
    {
        if (accept.Device) { message.DeviceName = learned.DeviceName; }
        if (accept.Group) { message.GroupIndex = learned.GroupIndex; }
        if (accept.Channel) { message.ChannelIndex = learned.ChannelIndex; }
        if (accept.Kind) { message.Kind = learned.Kind; }
        if (accept.Number) { message.Number = learned.Number; }
    }

    _Use_decl_annotations_
    void ApplyLearned(
        LearnedBinding const& learned,
        LearnAcceptance const& accept,
        FeedbackBinding& feedback) noexcept
    {
        if (accept.Device) { feedback.DeviceName = learned.DeviceName; }
        if (accept.Group) { feedback.GroupIndex = learned.GroupIndex; }
        if (accept.Channel) { feedback.ChannelIndex = learned.ChannelIndex; }
        if (accept.Kind) { feedback.Kind = learned.Kind; }
        if (accept.Number) { feedback.Number = learned.Number; }

        feedback.Enabled = true;
    }

    _Use_decl_annotations_
    bool IsSameBinding(LearnedBinding const& left, LearnedBinding const& right) noexcept
    {
        return left.DeviceName == right.DeviceName &&
            left.Kind == right.Kind &&
            left.GroupIndex == right.GroupIndex &&
            left.ChannelIndex == right.ChannelIndex &&
            left.Number == right.Number;
    }
}
