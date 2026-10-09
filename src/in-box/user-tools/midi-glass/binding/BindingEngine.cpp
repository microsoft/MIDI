// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK.

#include "BindingEngine.h"
#include "MackieControl.h"

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
        constexpr uint8_t StatusAssignablePerNoteController = 0x1;
        constexpr uint8_t StatusPerNotePitchBend = 0x6;

        // Portamento Control: the note the next note on glides from.
        constexpr uint8_t PortamentoControlNumber = 84;

        // Registered controller bank 0, index 7: the sensitivity of per-note pitch bend.
        constexpr uint8_t PerNoteBendRangeIndex = 7;

        // System real time, whole status bytes rather than nibbles.
        constexpr uint8_t StatusTimingClock = 0xF8;
        constexpr uint8_t StatusStart = 0xFA;
        constexpr uint8_t StatusContinue = 0xFB;
        constexpr uint8_t StatusStop = 0xFC;

        constexpr uint32_t MessageTypeMidi1ChannelVoice = 0x2;
        constexpr uint32_t MessageTypeMidi2ChannelVoice = 0x4;

        // System common and system real time. A clock generator's whole output is this type.
        constexpr uint32_t MessageTypeSystem = 0x1;

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
            case MessageKind::AssignablePerNoteController:
            case MessageKind::RegisteredController:
            case MessageKind::AssignedController:
                return true;

            default:
                return false;
            }
        }

        // The low half of a MIDI 2.0 note's second word. A note with no attribute type carries none.
        uint32_t AttributeDataOf(_In_ PreparedMessage const& message) noexcept
        {
            return message.AttributeType == 0 ? 0u : message.AttributeData;
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
                static_cast<uint8_t>(message.Number & 0x7F), message.AttributeType,
                (FieldValue(message, value, 16) << 16) | AttributeDataOf(message), words);
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
        case MessageKind::AssignablePerNoteController:
            BuildMidi2ChannelVoice(group,
                message.Kind == MessageKind::PerNoteController ? StatusPerNoteController : StatusAssignablePerNoteController,
                channel, static_cast<uint8_t>(message.Number & 0x7F), message.Controller,
                FieldValue(message, value, 32), words);
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
    uint32_t BuildNoteWords(
        PreparedMessage const& message,
        bool isOn,
        double velocity,
        uint32_t* words) noexcept
    {
        if (message.Kind != MessageKind::Note)
        {
            return 0;
        }

        // A note off lands on the bottom of the row's range, as it always has.
        auto const position = isOn && std::isfinite(velocity) ? std::clamp(velocity, 0.0, 1.0) : 0.0;

        auto const status = isOn ? StatusNoteOn : StatusNoteOff;
        auto const note = static_cast<uint8_t>(message.Number & 0x7F);

        if (message.UseMidi1Protocol)
        {
            auto seven = static_cast<uint8_t>(FieldValue(message, position, 7));

            if (isOn && seven == 0)
            {
                seven = 1;
            }

            words[0] = BuildMidi1ChannelVoice(message.GroupIndex, status, message.ChannelIndex, note, seven);
            return 1;
        }

        auto sixteen = FieldValue(message, position, 16);

        // Anything under this folds to zero when the service takes it down to seven bits for a
        // MIDI 1.0 device, and there a note on at zero is a note off.
        constexpr uint32_t QuietestNoteOn = 0x0200;

        if (isOn && sixteen < QuietestNoteOn)
        {
            sixteen = QuietestNoteOn;
        }

        BuildMidi2ChannelVoice(message.GroupIndex, status, message.ChannelIndex, note, message.AttributeType,
            (sixteen << 16) | AttributeDataOf(message), words);
        return 2;
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
            m_tempoWatchers.clear();
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
                    prepared.SwitchPositions = control.Kind == ControlKind::Switch ? SwitchPositionCount(control) : 0;

                    for (auto const& message : control.Messages)
                    {
                        if (message.Kind == MessageKind::Unrecognized)
                        {
                            continue;
                        }

                        auto const protocol = document.ProtocolOf(message.DeviceName);

                        // Only a function reaches a Mackie Control device, and a function means
                        // nothing anywhere else. A plain row there waits for somebody to pick one.
                        if (protocol == DeviceProtocol::MackieControl || message.Kind == MessageKind::MackieControl)
                        {
                            if (protocol == DeviceProtocol::MackieControl && message.Kind == MessageKind::MackieControl)
                            {
                                PrepareMackieRow(control.Kind, message, indexOf(message.DeviceName), prepared);
                            }

                            continue;
                        }

                        PreparedMessage entry{};

                        entry.Trigger = message.Trigger;
                        entry.Kind = message.Kind;
                        entry.DestinationIndex = indexOf(message.DeviceName);
                        entry.GroupIndex = static_cast<uint8_t>(
                            message.GroupIndex == AllGroups ? 0 : (message.GroupIndex & 0x0F));
                        entry.ChannelIndex = static_cast<uint8_t>(message.ChannelIndex & 0x0F);
                        entry.Number = static_cast<uint16_t>(message.Number);
                        entry.Controller = static_cast<uint8_t>((std::min)(message.Controller, MaximumPerNoteController));
                        entry.AttributeType = static_cast<uint8_t>((std::min)(message.AttributeType, MaximumAttributeType));
                        entry.AttributeData = static_cast<uint16_t>((std::min)(message.AttributeData, MaximumAttributeData));
                        entry.Minimum = message.Minimum;
                        entry.Maximum = message.Maximum;
                        entry.Detents = message.Detents;
                        entry.Axis = message.Axis;
                        entry.UseMidi1Protocol = SendsAsMidi1(message, protocol);
                        entry.Position = message.Position;

                        m_messages.push_back(entry);
                    }

                    prepared.MessageCount = static_cast<uint32_t>(m_messages.size()) - prepared.FirstMessage;

                    if (control.Feedback.Enabled && control.Feedback.Kind != MessageKind::Unrecognized)
                    {
                        PreparedFeedback feedback{};

                        feedback.ControlIndex = m_controls.size();
                        feedback.DestinationIndex = indexOf(control.Feedback.DeviceName);
                        feedback.Mode = control.Feedback.Mode;
                        feedback.Kind = control.Feedback.Kind;
                        feedback.GroupIndex = static_cast<uint8_t>(
                            control.Feedback.GroupIndex == AllGroups ? 0 : (control.Feedback.GroupIndex & 0x0F));
                        feedback.ChannelIndex = static_cast<uint8_t>(control.Feedback.ChannelIndex & 0x0F);
                        feedback.Number = static_cast<uint16_t>(control.Feedback.Number);
                        feedback.AnyGroup = control.Feedback.GroupIndex == AllGroups;
                        feedback.AnyChannel = !control.Feedback.MatchesChannel;
                        feedback.TempoFromWire = control.Feedback.TempoControlId.empty();

                        m_feedback.push_back(feedback);

                        if (control.Feedback.Mode == FeedbackMode::Tempo)
                        {
                            TempoWatcher watcher{};

                            watcher.ControlIndex = m_controls.size();
                            watcher.ClockControlId = control.Feedback.TempoControlId;

                            m_tempoWatchers.push_back(watcher);
                        }
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
            m_tempoWatchers.clear();
            m_startupOrder.clear();
        }
    }

    _Use_decl_annotations_
    void BindingEngine::PrepareMackieRow(
        ControlKind controlKind,
        ControlMessage const& message,
        int32_t destinationIndex,
        PreparedControl& prepared)
    {
        auto const function = message.Number;
        auto const group = static_cast<uint8_t>(message.GroupIndex == AllGroups ? 0 : (message.GroupIndex & 0x0F));

        PreparedMessage base{};

        base.DestinationIndex = destinationIndex;
        base.GroupIndex = group;
        base.UseMidi1Protocol = true;
        base.IsMackie = true;

        // A press is a note on at 127 and a release is a note on at 0, never a note off.
        auto const addBang = [this, &base, group](MessageTrigger trigger, uint32_t note, bool down)
            {
                auto entry = base;

                entry.Trigger = trigger;
                entry.Kind = MessageKind::RawUmp;
                entry.FixedWord = BuildMidi1ChannelVoice(
                    group, StatusNoteOn, 0, static_cast<uint8_t>(note), down ? uint8_t{ 127 } : uint8_t{ 0 });

                m_messages.push_back(entry);
            };

        auto const addFeedback = [this, destinationIndex, group](MessageKind kind, uint8_t channel, uint32_t number)
            {
                PreparedFeedback feedback{};

                feedback.ControlIndex = m_controls.size();
                feedback.DestinationIndex = destinationIndex;
                feedback.Mode = FeedbackMode::Message;
                feedback.Kind = kind;
                feedback.GroupIndex = group;
                feedback.ChannelIndex = channel;
                feedback.Number = static_cast<uint16_t>(number);
                feedback.IsMackie = true;

                m_feedback.push_back(feedback);
            };

        switch (ShapeOfMackieFunction(function))
        {
        case MackieShape::Button:
            // A latching control changes once per press, and the DAW needs a whole press each time.
            if (controlKind == ControlKind::Toggle)
            {
                for (auto const trigger : { MessageTrigger::TurnsOn, MessageTrigger::TurnsOff })
                {
                    addBang(trigger, function, true);
                    addBang(trigger, function, false);
                }
            }
            else
            {
                addBang(MessageTrigger::TurnsOn, function, true);
                addBang(MessageTrigger::TurnsOff, function, false);
            }

            addFeedback(MessageKind::Note, 0, function);
            break;

        case MackieShape::Fader:
        {
            auto const strip = MackieStripOf(function);
            auto entry = base;

            entry.Trigger = MessageTrigger::Changes;
            entry.Kind = MessageKind::PitchBend;
            entry.ChannelIndex = static_cast<uint8_t>(strip);

            m_messages.push_back(entry);

            addBang(MessageTrigger::Touched, MackieFaderTouchNote + strip, true);
            addBang(MessageTrigger::Released, MackieFaderTouchNote + strip, false);

            addFeedback(MessageKind::PitchBend, static_cast<uint8_t>(strip), 0);
            break;
        }

        case MackieShape::Encoder:
        {
            auto entry = base;

            entry.Trigger = MessageTrigger::Changes;
            entry.Kind = MessageKind::ControlChange;
            entry.Number = static_cast<uint16_t>(function == MackieJog
                ? MackieJogController
                : MackieVPotController + MackieStripOf(function));
            entry.IsRelative = true;

            m_messages.push_back(entry);

            prepared.HasRelative = true;
            break;
        }

        default:
            break;
        }
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::Evaluate(
        size_t controlIndex,
        MessageTrigger trigger,
        double value,
        std::span<PreparedSend> sends) const noexcept
    {
        return EvaluateAxis(controlIndex, trigger, value, ValueAxis::X, sends);
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluateAxis(
        size_t controlIndex,
        MessageTrigger trigger,
        double value,
        ValueAxis axis,
        std::span<PreparedSend> sends) const noexcept
    {
        return EvaluateRows(controlIndex, trigger, value, axis, NoteGate::FromValue, false, sends);
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluatePress(
        size_t controlIndex,
        MessageTrigger trigger,
        bool isOn,
        double velocity,
        std::span<PreparedSend> sends) const noexcept
    {
        return EvaluateRows(
            controlIndex,
            trigger,
            isOn ? velocity : 0.0,
            ValueAxis::X,
            isOn ? NoteGate::On : NoteGate::Off,
            false,
            sends);
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluateRows(
        size_t controlIndex,
        MessageTrigger trigger,
        double value,
        ValueAxis axis,
        NoteGate gate,
        bool atStartup,
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

            if (message.Axis != axis)
            {
                continue;
            }

            // A turn is sent by EvaluateRelative, from how far the control moved.
            if (message.IsRelative || (atStartup && message.IsMackie))
            {
                continue;
            }

            // A row for one position of a switch goes out only when the switch lands there, and
            // then it sends its maximum, the way a button sends its on value.
            auto rowValue = value;

            if (message.Position >= 0)
            {
                if (control.SwitchPositions < MinimumSwitchPositions ||
                    SwitchPositionAt(value, control.SwitchPositions) != message.Position)
                {
                    continue;
                }

                rowValue = 1.0;
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

            if (message.FixedWord != 0)
            {
                send.Words[0] = message.FixedWord;
                send.WordCount = 1;
            }
            else
            {
                send.WordCount = message.Kind == MessageKind::Note && gate != NoteGate::FromValue
                    ? BuildNoteWords(message, gate == NoteGate::On, rowValue, send.Words)
                    : BuildMessageWords(message, rowValue, send.Words);
            }

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
    uint32_t BindingEngine::EvaluateNote(
        size_t controlIndex,
        uint16_t note,
        double velocity,
        bool isOn,
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
            auto entry = m_messages[control.FirstMessage + i];

            if (entry.Kind != MessageKind::Note)
            {
                continue;
            }

            if (entry.DestinationIndex < 0 ||
                static_cast<size_t>(entry.DestinationIndex) >= m_destinations.size())
            {
                continue;
            }

            if (!m_destinations[static_cast<size_t>(entry.DestinationIndex)].IsAvailable)
            {
                continue;
            }

            // The key decides the note. Everything else about the row still applies, so a
            // keyboard pointed at two devices on two channels still sends to both.
            entry.Number = note;

            auto& send = sends[written];

            // On or off is the key's to say. Reading it from the velocity, as a note row bound to
            // a fader does, turned every key struck softer than half way into a note off.
            send.WordCount = BuildNoteWords(entry, isOn, velocity, send.Words);

            if (send.WordCount == 0)
            {
                continue;
            }

            send.DestinationIndex = entry.DestinationIndex;
            ++written;
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t PerNotePitchBendValue(double semitones, double rangeSemitones) noexcept
    {
        if (!std::isfinite(semitones) || !std::isfinite(rangeSemitones) || rangeSemitones <= 0.0)
        {
            return 0x80000000u;
        }

        auto const fraction = std::clamp(semitones / rangeSemitones, -1.0, 1.0);

        // Centered on 0x80000000, with a whole range either way. The top is one short of what
        // the arithmetic asks for, because that is the largest thing thirty two bits can hold.
        auto const value = std::llround(2147483648.0 + fraction * 2147483648.0);

        return static_cast<uint32_t>(std::clamp(value, 0LL, 4294967295LL));
    }

    _Use_decl_annotations_
    uint32_t SemitonesAsPitch725(double semitones) noexcept
    {
        if (!std::isfinite(semitones))
        {
            return 0;
        }

        return static_cast<uint32_t>(std::llround(std::clamp(semitones, 0.0, 127.0) * 33554432.0));
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluatePortamento(
        size_t controlIndex,
        uint16_t fromNote,
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
            auto const& entry = m_messages[control.FirstMessage + i];

            if (entry.Kind != MessageKind::Note)
            {
                continue;
            }

            if (entry.DestinationIndex < 0 ||
                static_cast<size_t>(entry.DestinationIndex) >= m_destinations.size() ||
                !m_destinations[static_cast<size_t>(entry.DestinationIndex)].IsAvailable)
            {
                continue;
            }

            auto& send = sends[written];
            auto const note = static_cast<uint8_t>(fromNote & 0x7F);

            if (entry.UseMidi1Protocol)
            {
                send.Words[0] = BuildMidi1ChannelVoice(
                    entry.GroupIndex, StatusControlChange, entry.ChannelIndex, PortamentoControlNumber, note);
                send.WordCount = 1;
            }
            else
            {
                // The UMP specification puts the note in the top seven bits of the value and
                // says the rest is ignored.
                BuildMidi2ChannelVoice(
                    entry.GroupIndex, StatusControlChange, entry.ChannelIndex, PortamentoControlNumber, 0,
                    static_cast<uint32_t>(note) << 25, send.Words);
                send.WordCount = 2;
            }

            send.DestinationIndex = entry.DestinationIndex;
            ++written;
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluatePerNotePitchBend(
        size_t controlIndex,
        uint16_t note,
        double semitones,
        double rangeSemitones,
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
            auto const& entry = m_messages[control.FirstMessage + i];

            if (entry.Kind != MessageKind::Note || entry.UseMidi1Protocol)
            {
                continue;
            }

            if (entry.DestinationIndex < 0 ||
                static_cast<size_t>(entry.DestinationIndex) >= m_destinations.size() ||
                !m_destinations[static_cast<size_t>(entry.DestinationIndex)].IsAvailable)
            {
                continue;
            }

            auto& send = sends[written];

            BuildMidi2ChannelVoice(
                entry.GroupIndex, StatusPerNotePitchBend, entry.ChannelIndex,
                static_cast<uint8_t>(note & 0x7F), 0,
                PerNotePitchBendValue(semitones, rangeSemitones), send.Words);

            send.WordCount = 2;
            send.DestinationIndex = entry.DestinationIndex;
            ++written;
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluatePerNoteBendRange(
        size_t controlIndex,
        double rangeSemitones,
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
            auto const& entry = m_messages[control.FirstMessage + i];

            if (entry.Kind != MessageKind::Note || entry.UseMidi1Protocol)
            {
                continue;
            }

            if (entry.DestinationIndex < 0 ||
                static_cast<size_t>(entry.DestinationIndex) >= m_destinations.size() ||
                !m_destinations[static_cast<size_t>(entry.DestinationIndex)].IsAvailable)
            {
                continue;
            }

            auto& send = sends[written];

            BuildMidi2ChannelVoice(
                entry.GroupIndex, StatusRegisteredController, entry.ChannelIndex,
                0, PerNoteBendRangeIndex,
                SemitonesAsPitch725(rangeSemitones), send.Words);

            send.WordCount = 2;
            send.DestinationIndex = entry.DestinationIndex;
            ++written;
        }

        return written;
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluateSystemRealTime(
        size_t controlIndex,
        uint8_t status,
        std::span<PreparedSend> sends) const noexcept
    {
        if (controlIndex >= m_controls.size() || sends.empty())
        {
            return 0;
        }

        auto const& control = m_controls[controlIndex];

        uint32_t written{ 0 };

        // One send per destination, not per row. A clock pointed at the same device twice would
        // otherwise run that device at double speed.
        bool alreadySent[MaximumDevicesPerLayout]{};

        for (uint32_t i = 0; i < control.MessageCount && written < sends.size(); ++i)
        {
            auto const& entry = m_messages[control.FirstMessage + i];

            if (entry.DestinationIndex < 0 ||
                static_cast<size_t>(entry.DestinationIndex) >= m_destinations.size())
            {
                continue;
            }

            auto const slot = static_cast<size_t>(entry.DestinationIndex);

            if (slot >= MaximumDevicesPerLayout || alreadySent[slot])
            {
                continue;
            }

            if (!m_destinations[slot].IsAvailable)
            {
                continue;
            }

            alreadySent[slot] = true;

            auto& send = sends[written];

            // A system real time message is one word: type 1, group, then the status byte.
            // It carries no channel and nothing to scale.
            send.Words[0] = (MessageTypeSystem << 28)
                | (static_cast<uint32_t>(entry.GroupIndex & 0x0F) << 24)
                | (static_cast<uint32_t>(status) << 16);

            send.WordCount = 1;
            send.DestinationIndex = entry.DestinationIndex;
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
    bool BindingEngine::TryDescribeValue(
        size_t controlIndex,
        ValueAxis axis,
        double position,
        uint32_t& value,
        bool& isAbsolute) const noexcept
    {
        value = 0;
        isAbsolute = false;

        if (controlIndex >= m_controls.size())
        {
            return false;
        }

        auto const& control = m_controls[controlIndex];

        for (uint32_t i = 0; i < control.MessageCount; ++i)
        {
            auto const& message = m_messages[control.FirstMessage + i];

            if (!IsChannelVoice(message.Kind) || message.Axis != axis || message.IsRelative)
            {
                continue;
            }

            auto const bits = FieldBitsFor(message);

            value = InterpolateValue(message, position, bits);

            // A MIDI 1.0 device, or either end being an exact number, means the customer is
            // working in a device's own units, and that is the number they want to see. A DAW
            // fader reads as a percentage whatever it is carried in.
            isAbsolute = !message.IsMackie && (
                message.UseMidi1Protocol ||
                message.Minimum.Scaling == ValueScaling::Absolute ||
                message.Maximum.Scaling == ValueScaling::Absolute);

            return true;
        }

        return false;
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

            written += EvaluateRows(index, MessageTrigger::Changes, control.DefaultValue, ValueAxis::X,
                NoteGate::FromValue, true, sends.subspan(written));
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
        bool blinks{ false };

        return TryResolveFeedback(words, wordCount, -1, controlIndex, value, blinks);
    }

    _Use_decl_annotations_
    bool BindingEngine::TryResolveFeedback(
        uint32_t const* words,
        uint32_t wordCount,
        int32_t destinationIndex,
        size_t& controlIndex,
        double& value,
        bool& blinks) const noexcept
    {
        controlIndex = 0;
        value = 0.0;
        blinks = false;

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

        // A note's velocity on the seven bit scale, which is what a Mackie Control light reads.
        uint32_t velocity{ 0 };

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
                velocity = status == StatusNoteOn ? data2 : 0;
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
                velocity = status == StatusNoteOn ? (words[1] >> 25) : 0;
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
            // Only a control watching for one particular message. An activity light and a
            // tempo light are answered somewhere else, and neither carries a value.
            if (feedback.Mode != FeedbackMode::Message)
            {
                continue;
            }

            // Note 94 from a keyboard is a note, not the DAW saying it is playing.
            if (feedback.IsMackie && (destinationIndex < 0 || feedback.DestinationIndex != destinationIndex))
            {
                continue;
            }

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

            if (feedback.IsMackie && kind == MessageKind::Note)
            {
                auto const light = MackieLightFromVelocity(velocity);

                value = light == MackieLight::On ? 1.0 : 0.0;
                blinks = light == MackieLight::Blinking;
            }

            return true;
        }

        return false;
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::EvaluateRelative(
        size_t controlIndex,
        int32_t ticks,
        std::span<PreparedSend> sends) const noexcept
    {
        if (controlIndex >= m_controls.size() || sends.empty() || ticks == 0)
        {
            return 0;
        }

        auto const& control = m_controls[controlIndex];

        uint32_t written{ 0 };

        for (uint32_t i = 0; i < control.MessageCount && written < sends.size(); ++i)
        {
            auto const& message = m_messages[control.FirstMessage + i];

            if (!message.IsRelative ||
                message.DestinationIndex < 0 ||
                static_cast<size_t>(message.DestinationIndex) >= m_destinations.size() ||
                !m_destinations[static_cast<size_t>(message.DestinationIndex)].IsAvailable)
            {
                continue;
            }

            auto& send = sends[written];

            send.Words[0] = BuildMidi1ChannelVoice(
                message.GroupIndex,
                StatusControlChange,
                message.ChannelIndex,
                static_cast<uint8_t>(message.Number & 0x7F),
                MackieTurnValue(ticks));

            send.WordCount = 1;
            send.DestinationIndex = message.DestinationIndex;
            ++written;
        }

        return written;
    }

    _Use_decl_annotations_
    bool BindingEngine::HasRelativeRows(size_t controlIndex) const noexcept
    {
        return controlIndex < m_controls.size() && m_controls[controlIndex].HasRelative;
    }

    _Use_decl_annotations_
    int32_t TakeRelativeTicks(double& baseline, double value) noexcept
    {
        if (!std::isfinite(value))
        {
            return 0;
        }

        if (!std::isfinite(baseline))
        {
            baseline = value;
            return 0;
        }

        auto const turned = std::trunc((value - baseline) * MackieTicksPerTravel);
        auto const ticks = static_cast<int32_t>(std::clamp(turned, -63.0, 63.0));

        baseline += ticks / MackieTicksPerTravel;

        return ticks;
    }

    _Use_decl_annotations_
    uint32_t BindingEngine::CollectFeedbackHits(
        uint32_t const* words,
        uint32_t wordCount,
        int32_t destinationIndex,
        std::span<FeedbackHit> hits) const noexcept
    {
        if (words == nullptr || wordCount == 0 || hits.empty())
        {
            return 0;
        }

        auto const messageType = (words[0] >> 28) & 0x0F;

        // Only the types that carry a group and a channel can be narrowed. Anything else counts
        // as traffic and nothing more, which is exactly what an activity light is for.
        auto const channelVoice =
            messageType == MessageTypeMidi1ChannelVoice ||
            messageType == MessageTypeMidi2ChannelVoice;

        auto const group = static_cast<uint8_t>((words[0] >> 24) & 0x0F);
        auto const channel = static_cast<uint8_t>((words[0] >> 16) & 0x0F);
        auto const status = static_cast<uint8_t>((words[0] >> 20) & 0x0F);

        // A system real time message keeps its whole status byte where a channel voice one
        // keeps a nibble, so the two are read from different places.
        auto const realTime = messageType == MessageTypeSystem
            ? static_cast<uint8_t>((words[0] >> 16) & 0xFF)
            : uint8_t{ 0 };

        // A MIDI 1.0 note on carrying velocity zero is how most gear says note off, so a light
        // watching for playing must not blink on it. MIDI 2.0 has a real note off and puts its
        // velocity in the second word, so this only applies to the one-word form.
        auto const velocityIsZero =
            messageType == MessageTypeMidi1ChannelVoice && (words[0] & 0x7F) == 0;

        auto const isNoteOn = channelVoice && status == StatusNoteOn && !velocityIsZero;
        auto const isControlChange = channelVoice && status == StatusControlChange;

        uint32_t written{ 0 };

        for (auto const& feedback : m_feedback)
        {
            if (written >= hits.size())
            {
                break;
            }

            // One specific message is answered by TryResolveFeedback, which carries the value.
            if (feedback.Mode == FeedbackMode::Message)
            {
                continue;
            }

            // A control naming no device takes traffic from anything on the layout, which is
            // what somebody dropping one lamp on a page expects it to do.
            if (feedback.DestinationIndex >= 0 && feedback.DestinationIndex != destinationIndex)
            {
                continue;
            }

            if (!feedback.AnyGroup && channelVoice && feedback.GroupIndex != group)
            {
                continue;
            }

            if (!feedback.AnyChannel && channelVoice && feedback.ChannelIndex != channel)
            {
                continue;
            }

            FeedbackHit hit{};

            hit.ControlIndex = feedback.ControlIndex;

            switch (feedback.Mode)
            {
            case FeedbackMode::AnyActivity:
                hit.Kind = FeedbackHitKind::Pulse;
                break;

            case FeedbackMode::Notes:
                // A note off is the end of something, not the start of it, so it does not
                // blink a light that is there to say somebody is playing.
                if (!isNoteOn)
                {
                    continue;
                }

                hit.Kind = FeedbackHitKind::Pulse;
                break;

            case FeedbackMode::ControlChanges:
                if (!isControlChange)
                {
                    continue;
                }

                hit.Kind = FeedbackHitKind::Pulse;
                break;

            case FeedbackMode::Transport:
                if (realTime == StatusStart || realTime == StatusContinue)
                {
                    hit.Kind = FeedbackHitKind::On;
                }
                else if (realTime == StatusStop)
                {
                    hit.Kind = FeedbackHitKind::Off;
                }
                else
                {
                    continue;
                }

                break;

            case FeedbackMode::Tempo:
                // Only a control following the wire. One following a clock on this layout is
                // driven by that clock rather than by anything arriving.
                if (!feedback.TempoFromWire || realTime != StatusTimingClock)
                {
                    continue;
                }

                hit.Kind = FeedbackHitKind::ClockTick;
                break;

            default:
                continue;
            }

            hits[written] = hit;
            ++written;
        }

        return written;
    }
}
