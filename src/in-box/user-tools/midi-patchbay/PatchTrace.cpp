// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "PatchTrace.h"

#include "RouteGraph.h"
#include "StatefulBlocks.h"

#include <algorithm>
#include <cwctype>
#include <memory>
#include <unordered_map>

namespace midipatchbay
{
    namespace
    {
        // The patch key the trace compiles under. It starts every counter name.
        constexpr wchar_t TraceKey[] = L"trace";

        // What comes after a throttle runs from inside the throttle here, so a chain of them is
        // a chain of calls. A patch can't loop through them, but this is never trusted.
        constexpr int32_t MaximumThrottleDepth = 16;

        void AddOnce(_Inout_ std::vector<std::wstring>& list, _In_ std::wstring const& id)
        {
            if (!id.empty() && std::find(list.begin(), list.end(), id) == list.end())
            {
                list.push_back(id);
            }
        }

        std::wstring Lowercase(_In_ std::wstring text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

            return text;
        }

        uint32_t ChannelVoiceWord(_In_ uint32_t type, _In_ uint8_t group, _In_ uint32_t status, _In_ uint8_t channel) noexcept
        {
            return (type << 28) | (static_cast<uint32_t>(group & 0x0F) << 24) | ((status & 0x0F) << 20) |
                (static_cast<uint32_t>(channel & 0x0F) << 16);
        }

        // Records where a message goes instead of sending it, and keeps what the engine keeps
        // from one message to the next.
        class TraceSink final : public RouteSink
        {
        public:
            TraceSink(
                _In_ RouteGraph const& graph,
                _In_ std::unordered_map<std::wstring, std::wstring> const& nodeOfDevice) :
                m_graph(graph),
                m_nodeOfDevice(nodeOfDevice),
                m_states(std::make_unique<BlockState[]>((std::max)(graph.States.size(), size_t{ 1 }))),
                m_memories(graph.Memories.size(), 0),
                m_seen(graph.Memories.size())
            {
                for (size_t i = 0; i < graph.States.size(); i++)
                {
                    PrepareBlockState(graph.States[i].Kind, m_states[i]);
                }
            }

            void Begin() noexcept
            {
                m_step = TraceStep{};
                m_message++;
            }

            TraceStep& Step() noexcept
            {
                return m_step;
            }

            LogicValue MemoryAt(_In_ size_t index) const noexcept
            {
                return index < m_memories.size() ? UnpackLogicValue(m_memories[index]) : LogicValue{};
            }

            void CountLink(_In_ uint32_t cell) noexcept override
            {
                try
                {
                    AddOnce(m_step.Links, ElementOf(cell));
                }
                catch (...)
                {
                }
            }

            void CountBlock(_In_ uint32_t cell, _In_ bool passed) noexcept override
            {
                try
                {
                    auto const id = ElementOf(cell);

                    AddOnce(m_step.Nodes, id);

                    if (!passed)
                    {
                        AddOnce(m_step.KeptOut, id);
                    }
                }
                catch (...)
                {
                }
            }

            void Send(
                _In_ uint32_t leaf,
                _In_reads_(wordCount) uint32_t const* words,
                _In_ uint8_t wordCount) noexcept override
            {
                try
                {
                    if (leaf >= m_graph.Leaves.size())
                    {
                        return;
                    }

                    auto const& device = m_graph.Leaves[leaf].DestinationDeviceId;
                    auto const found = m_nodeOfDevice.find(device);

                    TraceArrival arrival{};
                    arrival.EndpointId = found == m_nodeOfDevice.end() ? device : found->second;
                    arrival.Message.WordCount = static_cast<uint8_t>((std::min)(static_cast<size_t>(wordCount), arrival.Message.Words.size()));

                    std::copy_n(words, arrival.Message.WordCount, arrival.Message.Words.begin());

                    AddOnce(m_step.Nodes, arrival.EndpointId);
                    m_step.Arrivals.push_back(std::move(arrival));
                }
                catch (...)
                {
                }
            }

            // Routing paces what comes after a throttle on a thread of its own, with no tags. A
            // trace isn't about timing, so it goes on at once, with no tags.
            void Throttle(
                _In_ uint32_t throttle,
                _In_reads_(wordCount) uint32_t const* words,
                _In_ uint8_t wordCount) noexcept override
            {
                if (throttle >= m_graph.Throttles.size() || m_throttleDepth >= MaximumThrottleDepth)
                {
                    return;
                }

                auto const& entry = m_graph.Throttles[throttle];

                try
                {
                    AddOnce(m_step.Nodes, ElementOf(entry.Cell));
                }
                catch (...)
                {
                }

                m_throttleDepth++;

                for (uint32_t i = 0; i < entry.EdgeCount; i++)
                {
                    RunEdge(m_graph, m_graph.Edges[entry.FirstEdge + i], words, wordCount, *this);
                }

                m_throttleDepth--;
            }

            BlockState* StateOf(_In_ uint32_t state) noexcept override
            {
                return state < m_graph.States.size() ? &m_states[state] : nullptr;
            }

            // An LFO that follows a clock isn't running in a trace.
            void Clock(
                _In_ uint32_t target,
                _In_reads_(wordCount) uint32_t const* words,
                _In_ uint8_t wordCount) noexcept override
            {
                static_cast<void>(target);
                static_cast<void>(words);
                static_cast<void>(wordCount);
            }

            // Read once for each message, the way the engine reads it.
            LogicValue Memory(_In_ uint32_t memory) noexcept override
            {
                if (memory >= m_memories.size())
                {
                    return {};
                }

                if (m_seen[memory].second != m_message)
                {
                    m_seen[memory] = { m_memories[memory], m_message };
                }

                return UnpackLogicValue(m_seen[memory].first);
            }

            void ChangeMemory(
                _In_ uint32_t memory,
                _In_ SetMemorySettings const& settings,
                _In_ LogicValue const& source) noexcept override
            {
                if (memory >= m_memories.size())
                {
                    return;
                }

                m_memories[memory] = PackLogicValue(NextMemoryValue(settings, UnpackLogicValue(m_memories[memory]), source));
                m_seen[memory] = { m_memories[memory], m_message };
            }

        private:
            // "patch key|element id" back to the element id.
            std::wstring ElementOf(_In_ uint32_t cell) const
            {
                if (cell >= m_graph.Cells.size())
                {
                    return {};
                }

                auto const& name = m_graph.Cells[cell];
                auto const bar = name.find(L'|');

                return bar == std::wstring::npos ? name : name.substr(bar + 1);
            }

            RouteGraph const& m_graph;
            std::unordered_map<std::wstring, std::wstring> const& m_nodeOfDevice;

            std::unique_ptr<BlockState[]> m_states{};
            std::vector<uint64_t> m_memories{};
            std::vector<std::pair<uint64_t, uint64_t>> m_seen{};
            uint64_t m_message{ 0 };
            int32_t m_throttleDepth{ 0 };

            TraceStep m_step{};
        };
    }

    _Use_decl_annotations_
    TraceMessage MakeTraceMessage(
        TraceMessageKind kind,
        uint8_t group,
        uint8_t channel,
        uint32_t number,
        uint32_t value,
        bool midi2) noexcept
    {
        TraceMessage message{};

        uint32_t status{ 0x9 };

        switch (kind)
        {
        case TraceMessageKind::NoteOff:         status = 0x8; break;
        case TraceMessageKind::ControlChange:   status = 0xB; break;
        case TraceMessageKind::ProgramChange:   status = 0xC; break;
        case TraceMessageKind::PitchBend:       status = 0xE; break;
        case TraceMessageKind::ChannelPressure: status = 0xD; break;
        default:                                status = 0x9; break;
        }

        auto const seven = (std::min)(value, 127u);
        auto const fourteen = (std::min)(value, 16383u);
        auto const index = number & 0x7F;

        if (!midi2)
        {
            uint32_t first{ 0 };
            uint32_t second{ 0 };

            switch (kind)
            {
            case TraceMessageKind::ProgramChange:
                first = index;
                break;

            case TraceMessageKind::ChannelPressure:
                first = seven;
                break;

            case TraceMessageKind::PitchBend:
                first = fourteen & 0x7F;
                second = fourteen >> 7;
                break;

            default:
                first = index;
                second = seven;
                break;
            }

            message.Words[0] = ChannelVoiceWord(0x2, group, status, channel) | (first << 8) | second;
            message.WordCount = 1;

            return message;
        }

        message.Words[0] = ChannelVoiceWord(0x4, group, status, channel);
        message.WordCount = 2;

        switch (kind)
        {
        case TraceMessageKind::NoteOn:
        case TraceMessageKind::NoteOff:
            message.Words[0] |= index << 8;
            message.Words[1] = FieldValueOf(ScaledValue(seven, SevenBitRange), 16) << 16;
            break;

        case TraceMessageKind::ControlChange:
            message.Words[0] |= index << 8;
            message.Words[1] = FieldValueOf(ScaledValue(seven, SevenBitRange), 32);
            break;

        case TraceMessageKind::ProgramChange:
            message.Words[1] = index << 24;
            break;

        case TraceMessageKind::ChannelPressure:
            message.Words[1] = FieldValueOf(ScaledValue(seven, SevenBitRange), 32);
            break;

        case TraceMessageKind::PitchBend:
            message.Words[1] = FieldValueOf(ScaledValue(fourteen, FourteenBitRange), 32);
            break;

        default:
            break;
        }

        return message;
    }

    _Use_decl_annotations_
    std::optional<TraceMessage> ReadTraceWords(std::wstring_view text) noexcept
    {
        TraceMessage message{};
        size_t position{ 0 };

        auto const separator = [](wchar_t c) { return c == L' ' || c == L',' || c == L'\t'; };

        while (position < text.size())
        {
            while (position < text.size() && separator(text[position]))
            {
                position++;
            }

            if (position >= text.size())
            {
                break;
            }

            if (message.WordCount >= MaximumUmpWords)
            {
                return std::nullopt;
            }

            if (text.substr(position, 2) == L"0x" || text.substr(position, 2) == L"0X")
            {
                position += 2;
            }

            uint32_t value{ 0 };
            size_t digits{ 0 };

            while (position < text.size() && std::iswxdigit(text[position]))
            {
                if (digits >= 8)
                {
                    return std::nullopt;
                }

                auto const c = text[position];
                auto const digit = c <= L'9' ? c - L'0' : (c | 0x20) - L'a' + 10;

                value = (value << 4) | static_cast<uint32_t>(digit);
                digits++;
                position++;
            }

            if (digits == 0 || (position < text.size() && !separator(text[position])))
            {
                return std::nullopt;
            }

            message.Words[message.WordCount++] = value;
        }

        if (message.WordCount == 0)
        {
            return std::nullopt;
        }

        return message;
    }

    _Use_decl_annotations_
    TraceMessage WithTraceGroup(TraceMessage const& message, uint8_t group) noexcept
    {
        auto copy = message;

        if (copy.WordCount == 0)
        {
            return copy;
        }

        // Utility and stream messages have no group.
        auto const type = copy.Words[0] >> 28;

        if (type != 0x0 && type != 0xF)
        {
            copy.Words[0] = (copy.Words[0] & 0xF0FFFFFFu) | (static_cast<uint32_t>(group & 0x0F) << 24);
        }

        return copy;
    }

    _Use_decl_annotations_
    TraceResult TracePatch(
        PatchDocument const& patch,
        std::wstring const& sourceNodeId,
        std::vector<TraceMessage> const& messages) noexcept
    {
        TraceResult result{};

        try
        {
            RoutePatch routed{};
            routed.Key = TraceKey;
            routed.Patch = &patch;

            // Each endpoint stands for itself, so everything on the canvas is present.
            std::unordered_map<std::wstring, std::wstring> nodeOfDevice{};

            for (auto const& endpoint : patch.Endpoints)
            {
                routed.DeviceIds[endpoint.Id] = endpoint.Id;
                nodeOfDevice[Lowercase(endpoint.Id)] = endpoint.Id;
            }

            auto const graph = CompileRoutes({ routed });

            if (!graph.Problems.empty())
            {
                result.Outcome = TraceOutcome::DoesNotRoute;
                return result;
            }

            auto const source = Lowercase(sourceNodeId);

            auto const fromSource = std::any_of(graph.Roots.begin(), graph.Roots.end(),
                [&source](RouteRoot const& root) { return root.SourceDeviceId == source; });

            if (!fromSource)
            {
                result.Outcome = TraceOutcome::NothingFromSource;
                return result;
            }

            // A memory's name as the patch spells it, rather than the lowercase it is kept under.
            std::vector<std::wstring> tagNames{};
            std::vector<std::wstring> memoryNames{};

            for (auto const& block : patch.Blocks)
            {
                CollectLogicNames(block.Kind, block.Settings, tagNames, memoryNames);
            }

            std::vector<std::wstring> names{};

            for (auto const& key : graph.Memories)
            {
                auto const bar = key.find(L'|');
                auto name = bar == std::wstring::npos ? key : key.substr(bar + 1);

                for (auto const& spelled : memoryNames)
                {
                    if (SameLogicName(spelled, name))
                    {
                        name = spelled;
                        break;
                    }
                }

                names.push_back(std::move(name));
            }

            TraceSink sink{ graph, nodeOfDevice };

            for (size_t i = 0; i < messages.size() && i < MaximumTraceMessages; i++)
            {
                auto const& message = messages[i];

                sink.Begin();

                for (auto const& root : graph.Roots)
                {
                    if (root.SourceDeviceId == source)
                    {
                        RunRoot(graph, root, message.Words.data(), message.WordCount, sink);
                    }
                }

                auto& step = sink.Step();

                // The endpoint it came in from leads the way, when it went anywhere at all.
                if (!step.Links.empty())
                {
                    step.Nodes.insert(step.Nodes.begin(), sourceNodeId);
                }

                for (size_t m = 0; m < names.size(); m++)
                {
                    step.Memories.emplace_back(names[m], sink.MemoryAt(m));
                }

                result.Steps.push_back(std::move(step));
            }
        }
        catch (...)
        {
            result.Steps.clear();
            result.Outcome = TraceOutcome::DoesNotRoute;
        }

        return result;
    }
}
