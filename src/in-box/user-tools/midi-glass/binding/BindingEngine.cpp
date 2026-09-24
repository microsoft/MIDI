// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK.

#include "BindingEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace glass
{
    namespace
    {
        constexpr uint8_t StatusNoteOff = 0x8;
        constexpr uint8_t StatusNoteOn = 0x9;
        constexpr uint8_t StatusControlChange = 0xB;
        constexpr uint8_t StatusProgramChange = 0xC;
        constexpr uint8_t StatusChannelPressure = 0xD;
        constexpr uint8_t StatusPitchBend = 0xE;

        constexpr uint8_t StatusRegisteredController = 0x2;
        constexpr uint8_t StatusAssignedController = 0x3;
        constexpr uint8_t StatusPerNoteController = 0x0;

        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;

        bool IsChannelVoice(_In_ MessageKind kind) noexcept
        {
            switch (kind)
            {
            case MessageKind::Note:
            case MessageKind::ControlChange:
            case MessageKind::ProgramChange:
            case MessageKind::PitchBend:
            case MessageKind::ChannelPressure:
            case MessageKind::PerNoteController:
            case MessageKind::RegisteredController:
            case MessageKind::AssignedController:
                return true;

            default:
                return false;
            }
        }

        // The number that goes into a field of this width, for a control sitting at this position.
        uint32_t FieldValue(
            _In_ PreparedMessage const& message,
            _In_ double position,
            _In_ uint32_t bits) noexcept
        {
            return InterpolateValue(message, position, bits);
        }
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

    namespace
    {
        double FieldMaximum(_In_ uint32_t bits) noexcept
        {
            return (bits >= 32) ? 4294967295.0 : static_cast<double>((1u << bits) - 1u);
        }

        // A value in field units, without the final rounding, so step arithmetic does not
        // accumulate the error twice.
        double InFieldUnits(_In_ MessageValue const& value, _In_ uint32_t bits) noexcept
        {
            auto const maximum = FieldMaximum(bits);

            if (!std::isfinite(value.Value))
            {
                return 0.0;
            }

            return value.Scaling == ValueScaling::Absolute
                ? std::clamp(value.Value, 0.0, maximum)
                : std::clamp(value.Value, 0.0, 1.0) * maximum;
        }

        double StepInFieldUnits(_In_ MessageDetents const& detents, _In_ uint32_t bits) noexcept
        {
            return InFieldUnits({ detents.Step, detents.Scaling }, bits);
        }
    }

    _Use_decl_annotations_
    uint32_t ResolveEnd(MessageValue const& end, uint32_t bits) noexcept
    {
        return end.Scaling == ValueScaling::Absolute
            ? ClampToBits(end.Value, bits)
            : ScaleToBits(end.Value, bits);
    }

    _Use_decl_annotations_
    double DetentPosition(uint32_t index, uint32_t count) noexcept
    {
        if (count < 2)
        {
            return 0.0;
        }

        return std::clamp(static_cast<double>(index) / (count - 1), 0.0, 1.0);
    }

    _Use_decl_annotations_
    uint32_t DetentCount(PreparedMessage const& message, uint32_t bits) noexcept
    {
        switch (message.Detents.Mode)
        {
        case DetentMode::ExplicitValues:
            return static_cast<uint32_t>((std::min)(message.Detents.Stops.size(), MaximumDetentStops));

        case DetentMode::EvenSteps:
        {
            auto const step = StepInFieldUnits(message.Detents, bits);

            if (step <= 0.0)
            {
                return 0;
            }

            auto const span = std::fabs(
                InFieldUnits(message.Maximum, bits) - InFieldUnits(message.Minimum, bits));

            return static_cast<uint32_t>((std::min)(
                static_cast<double>(MaximumDetentStops),
                std::floor(span / step) + 1.0));
        }

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    uint32_t FieldBitsFor(PreparedMessage const& message) noexcept
    {
        if (message.UseMidi1Protocol)
        {
            return message.Kind == MessageKind::PitchBend ? 14u : 7u;
        }

        return message.Kind == MessageKind::Note ? 16u : 32u;
    }

    _Use_decl_annotations_
    uint32_t InterpolateValue(PreparedMessage const& message, double position, uint32_t bits) noexcept
    {
        if (!std::isfinite(position))
        {
            position = 0.0;
        }

        position = std::clamp(position, 0.0, 1.0);

        auto const maximum = FieldMaximum(bits);
        auto const low = InFieldUnits(message.Minimum, bits);
        auto const high = InFieldUnits(message.Maximum, bits);

        if (message.Detents.Mode == DetentMode::ExplicitValues && !message.Detents.Stops.empty())
        {
            auto const count = (std::min)(message.Detents.Stops.size(), MaximumDetentStops);
            auto const index = static_cast<size_t>(std::llround(position * (count - 1)));

            auto const stop = InFieldUnits(
                { message.Detents.Stops[(std::min)(index, count - 1)], message.Detents.Scaling }, bits);

            return static_cast<uint32_t>(std::llround(std::clamp(stop, 0.0, maximum)));
        }

        auto raw = low + position * (high - low);

        if (message.Detents.Mode == DetentMode::EvenSteps)
        {
            auto const step = StepInFieldUnits(message.Detents, bits);

            if (step > 0.0)
            {
                // Measured from the minimum, so a range that does not start at zero still has a
                // stop exactly on its own bottom end.
                auto const steps = std::llround((raw - low) / (high >= low ? step : -step));

                raw = low + static_cast<double>(steps) * (high >= low ? step : -step);
                raw = std::clamp(raw, (std::min)(low, high), (std::max)(low, high));
            }
        }

        return static_cast<uint32_t>(std::llround(std::clamp(raw, 0.0, maximum)));
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
    uint32_t BuildMessageWords(
        PreparedMessage const& message,
        double value,
        uint32_t* words) noexcept
    {
        if (!IsChannelVoice(message.Kind))
        {
            return 0;
        }

        if (message.UseMidi1Protocol)
        {
            return BuildMidi1ProtocolWords(message, value, words);
        }

        auto const group = message.GroupIndex;
        auto const channel = message.ChannelIndex;

        switch (message.Kind)
        {
        case MessageKind::Note:
        {
            auto const on = value >= 0.5;
            auto const status = on ? StatusNoteOn : StatusNoteOff;

            BuildMidi2ChannelVoice(group, status, channel,
                static_cast<uint8_t>(message.Number & 0x7F), 0,
                FieldValue(message, value, 16) << 16, words);
            return 2;
        }

        case MessageKind::ControlChange:
            BuildMidi2ChannelVoice(group, StatusControlChange, channel,
                static_cast<uint8_t>(message.Number & 0x7F), 0, FieldValue(message, value, 32), words);
            return 2;

        case MessageKind::ProgramChange:
            // A program number is an identity, not a position, so it is never scaled. Option
            // flags 0 means no bank change.
            BuildMidi2ChannelVoice(group, StatusProgramChange, channel, 0, 0,
                static_cast<uint32_t>(message.Number & 0x7F) << 24, words);
            return 2;

        case MessageKind::PitchBend:
            BuildMidi2ChannelVoice(group, StatusPitchBend, channel, 0, 0, FieldValue(message, value, 32), words);
            return 2;

        case MessageKind::ChannelPressure:
            BuildMidi2ChannelVoice(group, StatusChannelPressure, channel, 0, 0, FieldValue(message, value, 32), words);
            return 2;

        case MessageKind::RegisteredController:
        case MessageKind::AssignedController:
        {
            auto const status = message.Kind == MessageKind::RegisteredController
                ? StatusRegisteredController
                : StatusAssignedController;

            BuildMidi2ChannelVoice(group, status, channel,
                static_cast<uint8_t>((message.Number >> 7) & 0x7F),
                static_cast<uint8_t>(message.Number & 0x7F),
                FieldValue(message, value, 32), words);
            return 2;
        }

        case MessageKind::PerNoteController:
            BuildMidi2ChannelVoice(group, StatusPerNoteController, channel,
                static_cast<uint8_t>(message.Number & 0x7F), 0, FieldValue(message, value, 32), words);
            return 2;

        default:
            return 0;
        }
    }

    _Use_decl_annotations_
    uint32_t BuildMidi1ProtocolWords(
        PreparedMessage const& message,
        double value,
        uint32_t* words) noexcept
    {
        auto const group = message.GroupIndex;
        auto const channel = message.ChannelIndex;

        switch (message.Kind)
        {
        case MessageKind::Note:
        {
            auto const on = value >= 0.5;

            words[0] = BuildMidi1ChannelVoice(group, on ? StatusNoteOn : StatusNoteOff, channel,
                static_cast<uint8_t>(message.Number & 0x7F),
                static_cast<uint8_t>(FieldValue(message, value, 7)));
            return 1;
        }

        case MessageKind::ControlChange:
            words[0] = BuildMidi1ChannelVoice(group, StatusControlChange, channel,
                static_cast<uint8_t>(message.Number & 0x7F),
                static_cast<uint8_t>(FieldValue(message, value, 7)));
            return 1;

        case MessageKind::ProgramChange:
            words[0] = BuildMidi1ChannelVoice(group, StatusProgramChange, channel,
                static_cast<uint8_t>(message.Number & 0x7F), 0);
            return 1;

        case MessageKind::PitchBend:
        {
            auto const bend = FieldValue(message, value, 14);

            words[0] = BuildMidi1ChannelVoice(group, StatusPitchBend, channel,
                static_cast<uint8_t>(bend & 0x7F),
                static_cast<uint8_t>((bend >> 7) & 0x7F));
            return 1;
        }

        case MessageKind::ChannelPressure:
            words[0] = BuildMidi1ChannelVoice(group, StatusChannelPressure, channel,
                static_cast<uint8_t>(FieldValue(message, value, 7)), 0);
            return 1;

        default:
            // A registered or assigned controller becomes four control change messages, and a
            // per-note controller has no MIDI 1.0 form at all. The service does that expansion
            // on the way to a MIDI 1.0 client or device; this does not try to reproduce it.
            return 0;
        }
    }

    _Use_decl_annotations_
    void BindingEngine::Prepare(
        LayoutDocument const& document,
        std::vector<PreparedDestination> destinations) noexcept
    {
        try
        {
            m_destinations = std::move(destinations);
            m_controls.clear();
            m_messages.clear();
            m_feedback.clear();
            m_startupOrder.clear();

            m_controls.reserve(document.ControlCount());
            m_messages.reserve(document.ControlCount() * 2);

            auto const indexOf = [this](std::wstring const& name) noexcept -> int32_t
                {
                    for (size_t i = 0; i < m_destinations.size(); ++i)
                    {
                        if (m_destinations[i].Name == name)
                        {
                            return static_cast<int32_t>(i);
                        }
                    }

                    return -1;
                };

            std::vector<int32_t> keyboardOrder{};

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    PreparedControl prepared{};

                    prepared.FirstMessage = static_cast<uint32_t>(m_messages.size());
                    prepared.DefaultValue = static_cast<float>(control.DefaultValue);
                    prepared.SendsValueOnStart = control.SendsValueOnStart && !document.SuppressAllStartupValues;

                    for (auto const& message : control.Messages)
                    {
                        PreparedMessage entry{};

                        entry.Trigger = message.Trigger;
                        entry.Kind = message.Kind;
                        entry.DestinationIndex = indexOf(message.DeviceName);
                        entry.GroupIndex = static_cast<uint8_t>(
                            message.GroupIndex == AllGroups ? 0 : (message.GroupIndex & 0x0F));
                        entry.ChannelIndex = static_cast<uint8_t>(message.ChannelIndex & 0x0F);
                        entry.Number = static_cast<uint16_t>(message.Number);
                        entry.Minimum = message.Minimum;
                        entry.Maximum = message.Maximum;
                        entry.Detents = message.Detents;
                        entry.UseMidi1Protocol = message.UseMidi1Protocol;

                        m_messages.push_back(entry);
                    }

                    prepared.MessageCount = static_cast<uint32_t>(m_messages.size()) - prepared.FirstMessage;

                    if (control.Feedback.Enabled)
                    {
                        PreparedFeedback feedback{};

                        feedback.ControlIndex = m_controls.size();
                        feedback.DestinationIndex = indexOf(control.Feedback.DeviceName);
                        feedback.Kind = control.Feedback.Kind;
                        feedback.GroupIndex = static_cast<uint8_t>(
                            control.Feedback.GroupIndex == AllGroups ? 0 : (control.Feedback.GroupIndex & 0x0F));
                        feedback.ChannelIndex = static_cast<uint8_t>(control.Feedback.ChannelIndex & 0x0F);
                        feedback.Number = static_cast<uint16_t>(control.Feedback.Number);

                        m_feedback.push_back(feedback);
                    }

                    keyboardOrder.push_back(control.KeyboardOrder);
                    m_controls.push_back(prepared);
                }
            }

            // The startup pass runs in keyboard order, which is the order the person building the
            // layout could see and fix, not the order the controls happen to sit in the file.
            m_startupOrder.resize(m_controls.size());
            std::iota(m_startupOrder.begin(), m_startupOrder.end(), size_t{ 0 });

            std::stable_sort(m_startupOrder.begin(), m_startupOrder.end(),
                [&keyboardOrder](size_t left, size_t right)
                { return keyboardOrder[left] < keyboardOrder[right]; });
        }
        catch (...)
        {
            m_controls.clear();
            m_messages.clear();
            m_feedback.clear();
            m_startupOrder.clear();
        }
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::Evaluate(
        size_t controlIndex,
        MessageTrigger trigger,
        double value,
        std::span<PreparedSend> sends) const noexcept
    {
        if (controlIndex >= m_controls.size() || sends.empty())
        {
            return 0;
        }

        auto const& control = m_controls[controlIndex];

        uint32_t written{ 0 };

        for (uint32_t i = 0; i < control.MessageCount && written < sends.size(); ++i)
        {
            auto const& message = m_messages[control.FirstMessage + i];

            if (message.Trigger != trigger)
            {
                continue;
            }

            if (message.DestinationIndex < 0 ||
                static_cast<size_t>(message.DestinationIndex) >= m_destinations.size())
            {
                continue;
            }

            auto const& destination = m_destinations[static_cast<size_t>(message.DestinationIndex)];

            // Nothing is queued while a device is gone. Stale MIDI arriving late is worse than
            // nothing, and a layout that buffers a minute of fader moves would dump them all at
            // once the moment the device came back.
            if (!destination.IsAvailable)
            {
                continue;
            }

            auto& send = sends[written];

            send.WordCount = BuildMessageWords(message, value, send.Words);

            if (send.WordCount == 0)
            {
                continue;
            }

            send.DestinationIndex = message.DestinationIndex;
            ++written;
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::DetentCountForControl(size_t controlIndex) const noexcept
    {
        if (controlIndex >= m_controls.size())
        {
            return 0;
        }

        auto const& control = m_controls[controlIndex];

        uint32_t count{ 0 };

        for (uint32_t i = 0; i < control.MessageCount; ++i)
        {
            auto const& message = m_messages[control.FirstMessage + i];

            count = (std::max)(count, DetentCount(message, FieldBitsFor(message)));
        }

        return count;
    }

    _Use_decl_annotations_
    double BindingEngine::SnapToDetent(size_t controlIndex, double position) const noexcept
    {
        auto const count = DetentCountForControl(controlIndex);

        if (count < 2)
        {
            return std::clamp(position, 0.0, 1.0);
        }

        auto const clamped = std::clamp(position, 0.0, 1.0);
        auto const index = static_cast<uint32_t>(std::llround(clamped * (count - 1)));

        return DetentPosition(index, count);
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluateStartupValues(std::span<PreparedSend> sends) const noexcept
    {
        uint32_t written{ 0 };

        for (auto const index : m_startupOrder)
        {
            if (written >= sends.size())
            {
                break;
            }

            auto const& control = m_controls[index];

            if (!control.SendsValueOnStart)
            {
                continue;
            }

            written += Evaluate(index, MessageTrigger::Changes, control.DefaultValue,
                sends.subspan(written));
        }

        return written;
    }

    _Use_decl_annotations_
    bool BindingEngine::TryResolveFeedback(
        uint32_t const* words,
        uint32_t wordCount,
        size_t& controlIndex,
        double& value) const noexcept
    {
        controlIndex = 0;
        value = 0.0;

        if (words == nullptr || wordCount == 0)
        {
            return false;
        }

        auto const messageType = (words[0] >> 28) & 0x0F;
        auto const group = static_cast<uint8_t>((words[0] >> 24) & 0x0F);
        auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);
        auto const channel = static_cast<uint8_t>((words[0] >> 16) & 0x0F);

        MessageKind kind{};
        uint16_t number{ 0 };
        double incoming{ 0.0 };

        if (messageType == MessageTypeMidi1ChannelVoice)
        {
            auto const data1 = static_cast<uint8_t>((words[0] >> 8) & 0x7F);
            auto const data2 = static_cast<uint8_t>(words[0] & 0x7F);

            switch (status)
            {
            case StatusControlChange:
                kind = MessageKind::ControlChange;
                number = data1;
                incoming = data2 / 127.0;
                break;

            case StatusChannelPressure:
                kind = MessageKind::ChannelPressure;
                incoming = data1 / 127.0;
                break;

            case StatusPitchBend:
                kind = MessageKind::PitchBend;
                incoming = ((static_cast<uint32_t>(data2) << 7) | data1) / 16383.0;
                break;

            case StatusNoteOn:
            case StatusNoteOff:
                kind = MessageKind::Note;
                number = data1;
                incoming = (status == StatusNoteOn && data2 > 0) ? 1.0 : 0.0;
                break;

            default:
                return false;
            }
        }
        else if (messageType == MessageTypeMidi2ChannelVoice)
        {
            if (wordCount < 2)
            {
                return false;
            }

            auto const index1 = static_cast<uint8_t>((words[0] >> 8) & 0x7F);

            switch (status)
            {
            case StatusControlChange:
                kind = MessageKind::ControlChange;
                number = index1;
                incoming = words[1] / 4294967295.0;
                break;

            case StatusChannelPressure:
                kind = MessageKind::ChannelPressure;
                incoming = words[1] / 4294967295.0;
                break;

            case StatusPitchBend:
                kind = MessageKind::PitchBend;
                incoming = words[1] / 4294967295.0;
                break;

            case StatusNoteOn:
            case StatusNoteOff:
                kind = MessageKind::Note;
                number = index1;
                incoming = (status == StatusNoteOn && (words[1] >> 16) > 0) ? 1.0 : 0.0;
                break;

            default:
                return false;
            }
        }
        else
        {
            return false;
        }

        for (auto const& feedback : m_feedback)
        {
            if (feedback.Kind != kind ||
                feedback.GroupIndex != group ||
                feedback.ChannelIndex != channel)
            {
                continue;
            }

            // Pitch bend and channel pressure are per channel and carry no number, so matching on
            // one would never succeed.
            if ((kind == MessageKind::ControlChange || kind == MessageKind::Note) &&
                feedback.Number != number)
            {
                continue;
            }

            controlIndex = feedback.ControlIndex;
            value = incoming;

            return true;
        }

        return false;
    }
}
