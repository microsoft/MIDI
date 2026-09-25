// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, XAML and the MIDI SDK, so the unit tests compile it unchanged.

#include "ActionPlan.h"

#include <algorithm>

namespace glass
{
    namespace
    {
        constexpr uint32_t MessageTypeData64 = 0x3;

        constexpr uint8_t SysExStatusComplete = 0x0;
        constexpr uint8_t SysExStatusStart = 0x1;
        constexpr uint8_t SysExStatusContinue = 0x2;
        constexpr uint8_t SysExStatusEnd = 0x3;

        constexpr size_t SysExBytesPerPacket = 6;

        constexpr size_t TriggerSlot(_In_ MessageTrigger trigger) noexcept
        {
            return static_cast<size_t>(trigger);
        }

        // Which control this id is, counting every page in order then every control within a
        // page. The same index the binding engine and the surface use.
        int32_t ControlIndexOf(
            _In_ LayoutDocument const& document,
            _In_ std::wstring const& id) noexcept
        {
            if (id.empty())
            {
                return -1;
            }

            int32_t index{ 0 };

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    if (control.Id == id)
                    {
                        return index;
                    }

                    ++index;
                }
            }

            return -1;
        }

        int32_t PageIndexOf(
            _In_ LayoutDocument const& document,
            _In_ std::wstring const& id) noexcept
        {
            if (id.empty())
            {
                return -1;
            }

            for (size_t index = 0; index < document.Pages.size(); ++index)
            {
                if (document.Pages[index].Id == id)
                {
                    return static_cast<int32_t>(index);
                }
            }

            return -1;
        }

        int32_t DestinationIndexOf(
            _In_ std::vector<PreparedDestination> const& destinations,
            _In_ std::wstring const& name) noexcept
        {
            for (size_t index = 0; index < destinations.size(); ++index)
            {
                if (destinations[index].Name == name)
                {
                    return static_cast<int32_t>(index);
                }
            }

            return -1;
        }

        // A sequence step's message, resolved the same way the binding engine resolves a
        // control's. A step does not move, so its position is the top of its own range: that is
        // what makes "note 60 at velocity 100" mean 100 rather than something between the ends.
        PreparedMessage PrepareStepMessage(
            _In_ ControlMessage const& message,
            _In_ int32_t destinationIndex) noexcept
        {
            PreparedMessage prepared{};

            prepared.Trigger = message.Trigger;
            prepared.Kind = message.Kind;
            prepared.DestinationIndex = destinationIndex;
            prepared.GroupIndex = static_cast<uint8_t>(
                message.GroupIndex == AllGroups ? 0 : (message.GroupIndex & 0x0F));
            prepared.ChannelIndex = static_cast<uint8_t>(message.ChannelIndex & 0x0F);
            prepared.Number = static_cast<uint16_t>(message.Number);
            prepared.Minimum = message.Minimum;
            prepared.Maximum = message.Maximum;
            prepared.Detents = message.Detents;
            prepared.UseMidi1Protocol = message.UseMidi1Protocol;

            return prepared;
        }

        bool AppendBuiltWords(
            _In_ PreparedMessage const& prepared,
            _In_ double position,
            _Inout_ std::vector<PlanAction>& actions)
        {
            uint32_t words[4]{};

            auto const count = BuildMessageWords(prepared, position, words);

            if (count == 0)
            {
                return false;
            }

            PlanAction action{};

            action.Kind = ActionKind::Send;
            action.DestinationIndex = prepared.DestinationIndex;
            action.Words.assign(words, words + count);

            actions.push_back(std::move(action));

            return true;
        }
    }

    _Use_decl_annotations_
    bool PacketizeSystemExclusive(
        std::vector<uint8_t> const& bytes,
        uint8_t groupIndex,
        std::vector<uint32_t>& words) noexcept
    {
        words.clear();

        try
        {
            // A dump copied out of a manual may or may not carry its own wrapper. Either is
            // right, so both are accepted and neither reaches the wire: the UMP packet's own
            // status says where the message starts and ends.
            size_t first{ 0 };
            size_t last{ bytes.size() };

            if (last > first && bytes[first] == 0xF0)
            {
                ++first;
            }

            if (last > first && bytes[last - 1] == 0xF7)
            {
                --last;
            }

            auto const payload = last - first;

            if (payload == 0)
            {
                return false;
            }

            for (size_t i = first; i < last; ++i)
            {
                // A byte with its top bit set inside the payload is a status byte where data
                // belongs, which means the blob is not what somebody thinks it is. Sending the
                // part before it would leave the device waiting for an end that never comes.
                if (bytes[i] > 0x7F)
                {
                    return false;
                }
            }

            auto const packets = (payload + SysExBytesPerPacket - 1) / SysExBytesPerPacket;

            if (packets * 2 > MaximumPlanWords)
            {
                return false;
            }

            words.reserve(packets * 2);

            auto const group = static_cast<uint32_t>(groupIndex & 0x0F);

            for (size_t packet = 0; packet < packets; ++packet)
            {
                auto const offset = first + packet * SysExBytesPerPacket;
                auto const count = (std::min)(SysExBytesPerPacket, last - offset);

                uint8_t status{};

                if (packets == 1)
                {
                    status = SysExStatusComplete;
                }
                else if (packet == 0)
                {
                    status = SysExStatusStart;
                }
                else if (packet == packets - 1)
                {
                    status = SysExStatusEnd;
                }
                else
                {
                    status = SysExStatusContinue;
                }

                uint8_t data[SysExBytesPerPacket]{};

                for (size_t i = 0; i < count; ++i)
                {
                    data[i] = bytes[offset + i];
                }

                words.push_back(
                    (MessageTypeData64 << 28) |
                    (group << 24) |
                    (static_cast<uint32_t>(status) << 20) |
                    (static_cast<uint32_t>(count) << 16) |
                    (static_cast<uint32_t>(data[0]) << 8) |
                    static_cast<uint32_t>(data[1]));

                words.push_back(
                    (static_cast<uint32_t>(data[2]) << 24) |
                    (static_cast<uint32_t>(data[3]) << 16) |
                    (static_cast<uint32_t>(data[4]) << 8) |
                    static_cast<uint32_t>(data[5]));
            }

            return true;
        }
        catch (...)
        {
            words.clear();
            return false;
        }
    }

    namespace
    {
        // One message row turned into whatever it does beyond the immediate send. Appends
        // nothing for a channel voice message, which the binding engine already handles.
        void AppendMessageActions(
            _In_ LayoutDocument const& document,
            _In_ std::vector<PreparedDestination> const& destinations,
            _In_ ControlMessage const& message,
            _Inout_ std::vector<PlanAction>& actions,
            _Inout_ SequenceRunMode& mode,
            _In_ size_t depth);

        void AppendSequenceActions(
            _In_ LayoutDocument const& document,
            _In_ std::vector<PreparedDestination> const& destinations,
            _In_ Sequence const& sequence,
            _Inout_ std::vector<PlanAction>& actions,
            _In_ size_t depth)
        {
            // A sequence naming a sequence naming itself is a layout from a stranger, not a
            // mistake. The depth limit is what stops it being an infinite expansion.
            if (depth > 4)
            {
                return;
            }

            // Repeat blocks are flattened here rather than at run time, so the runner has no
            // control flow at all and a plan can be read straight off in the editor.
            size_t index{ 0 };

            while (index < sequence.Steps.size() && actions.size() < MaximumPlanActions)
            {
                auto const& step = sequence.Steps[index];

                // A step this build does not understand does nothing. Guessing at it would mean
                // sending a message the customer never asked for.
                if (!step.UnrecognizedKind.empty())
                {
                    ++index;
                    continue;
                }

                switch (step.Kind)
                {
                case SequenceStepKind::SendMidiMessage:
                case SequenceStepKind::SendSystemExclusive:
                {
                    // A channel voice message inside a sequence has to be built here. A control's
                    // own rows leave in the pointer handler, but nothing is holding a step.
                    auto const destination = DestinationIndexOf(destinations, step.Message.DeviceName);

                    if (step.Message.Kind == MessageKind::Note)
                    {
                        auto const prepared = PrepareStepMessage(step.Message, destination);

                        if (AppendBuiltWords(prepared, 1.0, actions))
                        {
                            // A note on with no note off behind it leaves a synthesizer droning,
                            // and a step list is exactly where that is easy to forget. The pair
                            // and the gap between them go in together.
                            PlanAction hold{};
                            hold.Kind = ActionKind::Wait;
                            hold.WaitMilliseconds = step.DurationMilliseconds;

                            actions.push_back(std::move(hold));

                            AppendBuiltWords(prepared, 0.0, actions);
                        }
                    }
                    else if (step.Message.Kind == MessageKind::SystemExclusive ||
                        step.Message.Kind == MessageKind::RawUmp ||
                        step.Message.Kind == MessageKind::Sequence ||
                        step.Message.Kind == MessageKind::GoToPage)
                    {
                        auto nested = SequenceRunMode::Once;

                        AppendMessageActions(document, destinations, step.Message, actions, nested, depth + 1);
                    }
                    else
                    {
                        AppendBuiltWords(PrepareStepMessage(step.Message, destination), 1.0, actions);
                    }

                    ++index;
                    break;
                }

                case SequenceStepKind::Wait:
                {
                    PlanAction action{};
                    action.Kind = ActionKind::Wait;
                    action.WaitMilliseconds = step.WaitMilliseconds;

                    actions.push_back(std::move(action));
                    ++index;
                    break;
                }

                case SequenceStepKind::SetControlValue:
                {
                    PlanAction action{};
                    action.Kind = ActionKind::SetControlValue;
                    action.TargetControlIndex = ControlIndexOf(document, step.TargetControlId);
                    action.TargetValue = std::clamp(step.TargetValue, 0.0, 1.0);

                    actions.push_back(std::move(action));
                    ++index;
                    break;
                }

                case SequenceStepKind::GoToPage:
                {
                    PlanAction action{};
                    action.Kind = ActionKind::GoToPage;
                    action.TargetPageIndex = PageIndexOf(document, step.Message.TargetPageId);

                    actions.push_back(std::move(action));
                    ++index;
                    break;
                }

                case SequenceStepKind::RepeatBlockStart:
                {
                    // Find the matching end, expand what is between it this many times, and
                    // carry on after it.
                    size_t nesting{ 1 };
                    size_t end{ index + 1 };

                    while (end < sequence.Steps.size() && nesting != 0)
                    {
                        if (sequence.Steps[end].Kind == SequenceStepKind::RepeatBlockStart) { ++nesting; }
                        else if (sequence.Steps[end].Kind == SequenceStepKind::RepeatBlockEnd) { --nesting; }

                        if (nesting != 0) { ++end; }
                    }

                    Sequence inner{};
                    inner.Steps.assign(
                        sequence.Steps.begin() + static_cast<ptrdiff_t>(index) + 1,
                        sequence.Steps.begin() + static_cast<ptrdiff_t>((std::min)(end, sequence.Steps.size())));

                    auto const times = std::clamp<uint32_t>(step.RepeatCount, 1, 256);

                    for (uint32_t pass = 0; pass < times && actions.size() < MaximumPlanActions; ++pass)
                    {
                        AppendSequenceActions(document, destinations, inner, actions, depth + 1);
                    }

                    index = (std::min)(end + 1, sequence.Steps.size());
                    break;
                }

                case SequenceStepKind::RepeatBlockEnd:
                case SequenceStepKind::HoldLayer:
                default:
                    ++index;
                    break;
                }
            }

            if (actions.size() > MaximumPlanActions)
            {
                actions.resize(MaximumPlanActions);
            }
        }

        _Use_decl_annotations_
        void AppendMessageActions(
            LayoutDocument const& document,
            std::vector<PreparedDestination> const& destinations,
            ControlMessage const& message,
            std::vector<PlanAction>& actions,
            SequenceRunMode& mode,
            size_t depth)
        {
            if (actions.size() >= MaximumPlanActions)
            {
                return;
            }

            auto const destination = DestinationIndexOf(destinations, message.DeviceName);
            auto const group = static_cast<uint8_t>(message.GroupIndex == AllGroups ? 0 : message.GroupIndex);

            switch (message.Kind)
            {
            case MessageKind::SystemExclusive:
            {
                PlanAction action{};
                action.Kind = ActionKind::Send;
                action.DestinationIndex = destination;

                if (!PacketizeSystemExclusive(message.SystemExclusive, group, action.Words))
                {
                    return;
                }

                actions.push_back(std::move(action));
                break;
            }

            case MessageKind::RawUmp:
            {
                if (message.RawWords.empty() || message.RawWords.size() > 4)
                {
                    return;
                }

                PlanAction action{};
                action.Kind = ActionKind::Send;
                action.DestinationIndex = destination;
                action.Words = message.RawWords;

                actions.push_back(std::move(action));
                break;
            }

            case MessageKind::Sequence:
            {
                auto const* const sequence = document.FindSequence(message.SequenceName);

                if (sequence != nullptr)
                {
                    mode = sequence->Mode;

                    AppendSequenceActions(document, destinations, *sequence, actions, depth + 1);
                }

                break;
            }

            case MessageKind::GoToPage:
            {
                PlanAction action{};
                action.Kind = ActionKind::GoToPage;
                action.TargetPageIndex = PageIndexOf(document, message.TargetPageId);

                actions.push_back(std::move(action));
                break;
            }

            default:
                // A channel voice message leaves in the pointer handler, with no clock in the
                // way. Putting it here as well would send it twice.
                break;
            }
        }
    }

    _Use_decl_annotations_
    void ActionPlanSet::Prepare(
        LayoutDocument const& document,
        std::vector<PreparedDestination> const& destinations) noexcept
    {
        Clear();

        try
        {
            auto const controlCount = document.ControlCount();

            m_index.assign(controlCount * TriggerCount, -1);

            size_t controlIndex{ 0 };

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    for (size_t slot = 0; slot < TriggerCount; ++slot)
                    {
                        auto const trigger = static_cast<MessageTrigger>(slot);

                        std::vector<PlanAction> actions{};
                        auto mode = SequenceRunMode::Once;

                        for (auto const& message : control.Messages)
                        {
                            if (message.Trigger == trigger)
                            {
                                AppendMessageActions(document, destinations, message, actions, mode, 0);
                            }
                        }

                        if (actions.empty())
                        {
                            continue;
                        }

                        ActionPlan plan{};
                        plan.Actions = std::move(actions);
                        plan.Loops = mode != SequenceRunMode::Once;
                        plan.StopsOnRelease = mode == SequenceRunMode::WhileHeld;

                        m_index[controlIndex * TriggerCount + slot] =
                            static_cast<int32_t>(m_plans.size());

                        m_plans.push_back(std::move(plan));
                    }

                    ++controlIndex;
                }
            }
        }
        catch (...)
        {
            Clear();
        }
    }

    _Use_decl_annotations_
    ActionPlan const* ActionPlanSet::Find(size_t controlIndex, MessageTrigger trigger) const noexcept
    {
        auto const slot = controlIndex * TriggerCount + TriggerSlot(trigger);

        if (slot >= m_index.size() || m_index[slot] < 0)
        {
            return nullptr;
        }

        return &m_plans[static_cast<size_t>(m_index[slot])];
    }

    void ActionPlanSet::Clear() noexcept
    {
        m_index.clear();
        m_plans.clear();
    }
}
