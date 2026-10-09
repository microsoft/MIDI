// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "LogicStepTests.h"
#include "TestMessages.h"

#include "LogicSteps.h"
#include "PatchSerializer.h"
#include "PatchTrace.h"
#include "RouteGraph.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOff = 0x8;
    constexpr uint8_t NoteOn = 0x9;
    constexpr uint8_t ControlChange = 0xB;
    constexpr uint8_t ProgramChange = 0xC;
    constexpr uint8_t PitchBend = 0xE;

    // Keeps step state and memories from one message to the next, and reads a memory once for
    // each message, the way the engine does.
    class LogicSink : public RouteSink
    {
    public:
        explicit LogicSink(RouteGraph const& graph) :
            m_graph(graph),
            m_states(std::make_unique<BlockState[]>((std::max)(graph.States.size(), size_t{ 1 }))),
            m_memories(graph.Memories.size(), 0),
            m_seen(graph.Memories.size())
        {
            for (size_t i = 0; i < graph.States.size(); i++)
            {
                PrepareBlockState(graph.States[i].Kind, m_states[i]);
            }
        }

        std::vector<std::pair<std::wstring, Message>> Sent{};

        void CountLink(uint32_t) noexcept override {}
        void CountBlock(uint32_t, bool) noexcept override {}
        void Clock(uint32_t, uint32_t const*, uint8_t) noexcept override {}

        // What comes after a throttle runs on another thread, with no tags.
        void Throttle(uint32_t throttle, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            auto const& entry = m_graph.Throttles[throttle];

            for (uint32_t i = 0; i < entry.EdgeCount; i++)
            {
                RunEdge(m_graph, m_graph.Edges[entry.FirstEdge + i], words, wordCount, *this);
            }
        }

        void Send(uint32_t leaf, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            Message message{};
            std::copy_n(words, wordCount, message.Words.begin());
            message.Count = wordCount;

            Sent.emplace_back(m_graph.Leaves[leaf].DestinationDeviceId, message);
        }

        BlockState* StateOf(uint32_t state) noexcept override
        {
            return state < m_graph.States.size() ? &m_states[state] : nullptr;
        }

        LogicValue Memory(uint32_t memory) noexcept override
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

        void ChangeMemory(uint32_t memory, SetMemorySettings const& settings, LogicValue const& source) noexcept override
        {
            if (memory >= m_memories.size())
            {
                return;
            }

            m_memories[memory] = PackLogicValue(NextMemoryValue(settings, UnpackLogicValue(m_memories[memory]), source));
            m_seen[memory] = { m_memories[memory], m_message };
        }

        void Arrive(std::wstring const& device, Message const& message)
        {
            m_message++;

            for (auto const& root : m_graph.Roots)
            {
                if (root.SourceDeviceId == device)
                {
                    RunRoot(m_graph, root, message.Words.data(), message.Count, *this);
                }
            }
        }

        // Everything sent to one device so far.
        std::vector<Message> Take(std::wstring const& device)
        {
            std::vector<Message> result{};

            for (auto const& [destination, message] : Sent)
            {
                if (destination == device)
                {
                    result.push_back(message);
                }
            }

            return result;
        }

        LogicValue MemoryNow(std::wstring const& key) const
        {
            for (size_t i = 0; i < m_graph.Memories.size(); i++)
            {
                if (m_graph.Memories[i] == key)
                {
                    return UnpackLogicValue(m_memories[i]);
                }
            }

            return {};
        }

    private:
        RouteGraph const& m_graph;
        std::unique_ptr<BlockState[]> m_states{};
        std::vector<uint64_t> m_memories{};
        std::vector<std::pair<uint64_t, uint64_t>> m_seen{};
        uint64_t m_message{ 0 };
    };

    void AddEndpoint(PatchDocument& patch, std::wstring const& id)
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        patch.Endpoints.push_back(endpoint);
    }

    PatchBlock& AddBlock(PatchDocument& patch, std::wstring const& id, BlockKind kind)
    {
        PatchBlock block{};
        block.Id = id;
        block.Kind = kind;
        block.Settings = DefaultBlockSettings(kind);
        patch.Blocks.push_back(block);
        return patch.Blocks.back();
    }

    void Link(PatchDocument& patch, std::wstring const& from, std::wstring const& to, int32_t way = AllGroups)
    {
        PatchConnection link{};
        link.Id = from + L">" + to + L"/" + std::to_wstring(way);
        link.SourceId = from;
        link.DestinationId = to;
        link.SourceGroupIndex = way;
        patch.Connections.push_back(link);
    }

    RouteGraph Compile(PatchDocument const& patch)
    {
        RoutePatch input{};
        input.Key = L"k";
        input.Patch = &patch;

        for (auto const& endpoint : patch.Endpoints)
        {
            input.DeviceIds[endpoint.Id] = L"dev-" + endpoint.Id;
        }

        return CompileRoutes({ input });
    }

    LogicSource FromPart(MessagePart part)
    {
        LogicSource source{};
        source.Kind = LogicSourceKind::Part;
        source.Place.Part = part;
        return source;
    }

    LogicSource FromName(LogicSourceKind kind, std::wstring const& name)
    {
        LogicSource source{};
        source.Kind = kind;
        source.Name = name;
        return source;
    }

    LogicSource FromNumber(uint32_t number, LogicUnit unit = LogicUnit::Number)
    {
        LogicSource source{};
        source.Kind = LogicSourceKind::Number;
        source.Number = number;
        source.Unit = unit;
        return source;
    }

    LogicCondition Condition(LogicTest test, uint32_t value)
    {
        LogicCondition condition{};
        condition.Test = test;
        condition.Value = value;
        return condition;
    }

    SwitchCase Case(int32_t id, LogicTest test, uint32_t value)
    {
        SwitchCase entry{};
        entry.Id = id;
        entry.Condition = Condition(test, value);
        return entry;
    }

    // A keyboard, two synths, and a Branch between them on velocity of at least 100 of 127.
    PatchDocument VelocitySplit()
    {
        PatchDocument patch{};
        AddEndpoint(patch, L"kb");
        AddEndpoint(patch, L"a");
        AddEndpoint(patch, L"b");

        auto& branch = AddBlock(patch, L"br", BlockKind::Branch);
        branch.Settings.Branch.Subject = FromPart(MessagePart::Velocity);
        branch.Settings.Branch.Unit = LogicUnit::Value;
        branch.Settings.Branch.Condition = Condition(LogicTest::AtLeast, static_cast<uint32_t>(HundredthsFromSevenBit(100)));

        Link(patch, L"kb", L"br");
        Link(patch, L"br", L"a", BranchYesWay);
        Link(patch, L"br", L"b", BranchNoWay);

        return patch;
    }

    // SysEx7 packets: complete 0, start 1, continue 2, end 3. The first data byte is the
    // second byte of the first word.
    Message SysEx7Packet(uint8_t status, uint8_t firstByte)
    {
        Message message{};
        message.Words[0] = (0x3u << 28) | (static_cast<uint32_t>(status & 0x0F) << 20) | (6u << 16) |
            (static_cast<uint32_t>(firstByte & 0x7F) << 8);
        message.Words[1] = 0x01020304u;
        message.Count = 2;
        return message;
    }
}

void LogicStepTests::ABranchSendsYesAndNoTheWayItTests()
{
    auto const patch = VelocitySplit();
    auto const graph = Compile(patch);

    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 110));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 62, 50));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());

    // Exactly the threshold goes Yes.
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 64, 100));
    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-a").size());

    // A control change has no velocity, so it goes every way, the way a pair of filters would.
    sink.Arrive(L"dev-kb", Midi1(0, ControlChange, 0, 1, 64));
    VERIFY_ARE_EQUAL(size_t{ 3 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::ANewBranchSendsEverythingYes()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    AddBlock(patch, L"br", BlockKind::Branch);

    Link(patch, L"kb", L"br");
    Link(patch, L"br", L"a", BranchYesWay);
    Link(patch, L"br", L"b", BranchNoWay);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 10));
    sink.Arrive(L"dev-kb", Midi1(0, ControlChange, 3, 7, 100));

    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::ANoteOffGoesWhereItsNoteOnWent()
{
    auto const patch = VelocitySplit();
    auto const graph = Compile(patch);

    LogicSink sink{ graph };

    // Played hard, so it went Yes. The note off is a note on at velocity zero, which would have
    // gone No by its own velocity.
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 120));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 0));

    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());

    // The same with MIDI 2.0, whose note off carries a release velocity of its own.
    sink.Arrive(L"dev-kb", Midi2(0, NoteOn, 1, 62, 0, 0xF0000000u));
    sink.Arrive(L"dev-kb", Midi2(0, NoteOff, 1, 62, 0, 0x10000000u));

    VERIFY_ARE_EQUAL(size_t{ 4 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());

    // A note off for a note the step never saw goes every way, which can only end something.
    sink.Arrive(L"dev-kb", Midi1(0, NoteOff, 2, 70, 0));

    VERIFY_ARE_EQUAL(size_t{ 5 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::APedalLetGoReachesEveryWayItWentDown()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"fc");
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    // The foot controller picks the scene and is kept out.
    auto& set = AddBlock(patch, L"set", BlockKind::SetMemory);
    set.Settings.SetMemory.Memory = L"Scene";
    set.Settings.SetMemory.Value = FromPart(MessagePart::Program);
    set.Settings.SetMemory.PassesTriggers = false;

    auto& choice = AddBlock(patch, L"sw", BlockKind::Switch);
    choice.Settings.Switch.Subject = FromName(LogicSourceKind::Memory, L"scene");
    choice.Settings.Switch.Unit = LogicUnit::Number;
    choice.Settings.Switch.Cases = { Case(1, LogicTest::Is, 0), Case(2, LogicTest::Is, 1) };

    Link(patch, L"fc", L"set");
    Link(patch, L"kb", L"sw");
    Link(patch, L"sw", L"a", 1);
    Link(patch, L"sw", L"b", 2);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-fc", Midi1(0, ProgramChange, 0, 0, 0));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));
    sink.Arrive(L"dev-kb", Midi1(0, ControlChange, 0, 64, 127));

    // The scene changes while the pedal and the note are held.
    sink.Arrive(L"dev-fc", Midi1(0, ProgramChange, 0, 1, 0));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 0));
    sink.Arrive(L"dev-kb", Midi1(0, ControlChange, 0, 64, 0));

    auto const toA = sink.Take(L"dev-a");
    auto const toB = sink.Take(L"dev-b");

    // A: the note on, the pedal down, the note off, the pedal up.
    VERIFY_ARE_EQUAL(size_t{ 4 }, toA.size());

    // B: only the pedal up, which is where the pedal would go now.
    VERIFY_ARE_EQUAL(size_t{ 1 }, toB.size());
    VERIFY_IS_TRUE(toB.front() == Midi1(0, ControlChange, 0, 64, 0));
}

void LogicStepTests::TagsStayWithTheirOwnPath()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    auto& first = AddBlock(patch, L"t1", BlockKind::SetTag);
    first.Settings.SetTag.Tag = L"T";
    first.Settings.SetTag.Value = FromNumber(1);

    // One path sets the tag again; the other must still see 1.
    auto& second = AddBlock(patch, L"t2", BlockKind::SetTag);
    second.Settings.SetTag.Tag = L"T";
    second.Settings.SetTag.Value = FromNumber(2);

    auto& isTwo = AddBlock(patch, L"b2", BlockKind::Branch);
    isTwo.Settings.Branch.Subject = FromName(LogicSourceKind::Tag, L"T");
    isTwo.Settings.Branch.Unit = LogicUnit::Number;
    isTwo.Settings.Branch.Condition = Condition(LogicTest::Is, 2);

    auto& isOne = AddBlock(patch, L"b1", BlockKind::Branch);
    isOne.Settings.Branch.Subject = FromName(LogicSourceKind::Tag, L"T");
    isOne.Settings.Branch.Unit = LogicUnit::Number;
    isOne.Settings.Branch.Condition = Condition(LogicTest::Is, 1);

    Link(patch, L"kb", L"t1");
    Link(patch, L"t1", L"t2");
    Link(patch, L"t1", L"b1");
    Link(patch, L"t2", L"b2");
    Link(patch, L"b2", L"a", BranchYesWay);
    Link(patch, L"b1", L"b", BranchYesWay);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::ATagLastsTheWholeTrip()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    auto& first = AddBlock(patch, L"ta", BlockKind::SetTag);
    first.Settings.SetTag.Tag = L"A";
    first.Settings.SetTag.Value = FromPart(MessagePart::Note);

    auto& second = AddBlock(patch, L"tb", BlockKind::SetTag);
    second.Settings.SetTag.Tag = L"B";
    second.Settings.SetTag.Value = FromNumber(2);

    // A transpose in between changes the note, not the tag.
    AddBlock(patch, L"tr", BlockKind::Transpose).Settings.Transform.TransposeSemitones = 12;

    auto& branch = AddBlock(patch, L"br", BlockKind::Branch);
    branch.Settings.Branch.Subject = FromName(LogicSourceKind::Tag, L"A");
    branch.Settings.Branch.Unit = LogicUnit::Note;
    branch.Settings.Branch.Condition = Condition(LogicTest::Is, 60);

    Link(patch, L"kb", L"ta");
    Link(patch, L"ta", L"tb");
    Link(patch, L"tb", L"tr");
    Link(patch, L"tr", L"br");
    Link(patch, L"br", L"a", BranchYesWay);
    Link(patch, L"br", L"b", BranchNoWay);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));

    auto const toA = sink.Take(L"dev-a");

    VERIFY_ARE_EQUAL(size_t{ 1 }, toA.size());
    VERIFY_IS_TRUE(toA.front() == Midi1(0, NoteOn, 0, 72, 100));
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::ATagKeepsWhatTheMessageWasBeforeAChange()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"piano");
    AddEndpoint(patch, L"strings");

    auto& tag = AddBlock(patch, L"tag", BlockKind::SetTag);
    tag.Settings.SetTag.Tag = L"Played on";
    tag.Settings.SetTag.Value = FromPart(MessagePart::Channel);

    auto& map = AddBlock(patch, L"map", BlockKind::ChannelMap);
    map.Settings.Transform.ChannelMap.fill(0);

    auto& choice = AddBlock(patch, L"sw", BlockKind::Switch);
    choice.Settings.Switch.Subject = FromName(LogicSourceKind::Tag, L"Played on");
    choice.Settings.Switch.Unit = LogicUnit::Channel;
    choice.Settings.Switch.Cases = { Case(1, LogicTest::Is, 0), Case(2, LogicTest::Is, 1) };

    Link(patch, L"kb", L"tag");
    Link(patch, L"tag", L"map");
    Link(patch, L"map", L"sw");
    Link(patch, L"sw", L"piano", 1);
    Link(patch, L"sw", L"strings", 2);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 1, 60, 100));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 62, 100));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 5, 64, 100));

    auto const strings = sink.Take(L"dev-strings");
    auto const piano = sink.Take(L"dev-piano");

    // Played on channel 2, sent on channel 1.
    VERIFY_ARE_EQUAL(size_t{ 1 }, strings.size());
    VERIFY_IS_TRUE(strings.front() == Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, piano.size());
    VERIFY_IS_TRUE(piano.front() == Midi1(0, NoteOn, 0, 62, 100));
}

void LogicStepTests::AnEmptyTagGoesTheUnreadableWay()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    auto& branch = AddBlock(patch, L"br", BlockKind::Branch);
    branch.Settings.Branch.Subject = FromName(LogicSourceKind::Tag, L"never set");
    branch.Settings.Branch.Unit = LogicUnit::Number;
    branch.Settings.Branch.Condition = Condition(LogicTest::Is, 1);

    Link(patch, L"kb", L"br");
    Link(patch, L"br", L"a", BranchYesWay);
    Link(patch, L"br", L"b", BranchNoWay);

    for (auto const& [way, toA, toB] : { std::tuple{ UnreadableWay::EveryWay, 1, 1 },
                                         std::tuple{ UnreadableWay::KeepOut, 0, 0 },
                                         std::tuple{ UnreadableWay::FirstWay, 1, 0 },
                                         std::tuple{ UnreadableWay::LastWay, 0, 1 } })
    {
        patch.Blocks.front().Settings.Branch.Unreadable = way;

        auto const graph = Compile(patch);
        LogicSink sink{ graph };

        sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));

        VERIFY_ARE_EQUAL(static_cast<size_t>(toA), sink.Take(L"dev-a").size());
        VERIFY_ARE_EQUAL(static_cast<size_t>(toB), sink.Take(L"dev-b").size());
    }

    // "Is empty" can look at an empty tag.
    patch.Blocks.front().Settings.Branch.Condition.Test = LogicTest::IsEmpty;
    patch.Blocks.front().Settings.Branch.Unreadable = UnreadableWay::KeepOut;

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
}

void LogicStepTests::AMemoryLastsBetweenMessages()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"fc");
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");
    AddEndpoint(patch, L"c");

    auto& set = AddBlock(patch, L"set", BlockKind::SetMemory);
    set.Settings.SetMemory.Memory = L"Scene";
    set.Settings.SetMemory.EveryMessage = false;
    set.Settings.SetMemory.Trigger.Kind = GateTriggerKind::ProgramChange;
    set.Settings.SetMemory.Value = FromPart(MessagePart::Program);
    set.Settings.SetMemory.PassesTriggers = false;

    auto& choice = AddBlock(patch, L"sw", BlockKind::Switch);
    choice.Settings.Switch.Subject = FromName(LogicSourceKind::Memory, L"Scene");
    choice.Settings.Switch.Unit = LogicUnit::Number;
    choice.Settings.Switch.Cases = { Case(1, LogicTest::Is, 0), Case(2, LogicTest::Is, 1) };

    // The foot controller's other messages go on to C.
    Link(patch, L"fc", L"set");
    Link(patch, L"set", L"c");
    Link(patch, L"kb", L"sw");
    Link(patch, L"sw", L"a", 1);
    Link(patch, L"sw", L"b", 2);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    // Nothing set yet: the note goes every way, as a message the Switch can't look at.
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());

    sink.Arrive(L"dev-fc", Midi1(0, ProgramChange, 0, 1, 0));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 62, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-b").size());

    sink.Arrive(L"dev-fc", Midi1(0, ProgramChange, 0, 0, 0));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 64, 100));

    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Take(L"dev-b").size());

    // The program changes were kept out, and what else the foot controller sent went on.
    sink.Arrive(L"dev-fc", Midi1(0, ControlChange, 0, 11, 90));

    auto const toC = sink.Take(L"dev-c");
    VERIFY_ARE_EQUAL(size_t{ 1 }, toC.size());
    VERIFY_IS_TRUE(toC.front() == Midi1(0, ControlChange, 0, 11, 90));

    VERIFY_ARE_EQUAL(0u, WholeOf(sink.MemoryNow(L"k|scene")));
}

void LogicStepTests::AMessageSeesItsOwnChangeToAMemory()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    auto& set = AddBlock(patch, L"set", BlockKind::SetMemory);
    set.Settings.SetMemory.Memory = L"Last";
    set.Settings.SetMemory.Value = FromPart(MessagePart::Note);

    auto& branch = AddBlock(patch, L"br", BlockKind::Branch);
    branch.Settings.Branch.Subject = FromName(LogicSourceKind::Memory, L"Last");
    branch.Settings.Branch.Unit = LogicUnit::Note;
    branch.Settings.Branch.Condition = Condition(LogicTest::AtLeast, 60);

    Link(patch, L"kb", L"set");
    Link(patch, L"set", L"br");
    Link(patch, L"br", L"a", BranchYesWay);
    Link(patch, L"br", L"b", BranchNoWay);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 72, 100));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 40, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::ToggleAndStepWalkTheirRange()
{
    SetMemorySettings toggle{};
    toggle.Action = MemoryAction::Toggle;
    toggle.First = 0;
    toggle.Second = 127;

    auto value = NextMemoryValue(toggle, {}, {});
    VERIFY_ARE_EQUAL(0u, WholeOf(value));

    value = NextMemoryValue(toggle, value, {});
    VERIFY_ARE_EQUAL(127u, WholeOf(value));

    value = NextMemoryValue(toggle, value, {});
    VERIFY_ARE_EQUAL(0u, WholeOf(value));

    SetMemorySettings up{};
    up.Action = MemoryAction::StepUp;
    up.Lowest = 1;
    up.Highest = 3;
    up.Wraps = true;

    std::vector<uint32_t> seen{};
    LogicValue current{};

    for (int i = 0; i < 4; i++)
    {
        current = NextMemoryValue(up, current, {});
        seen.push_back(WholeOf(current));
    }

    VERIFY_IS_TRUE((seen == std::vector<uint32_t>{ 1, 2, 3, 1 }));

    SetMemorySettings down{};
    down.Action = MemoryAction::StepDown;
    down.Lowest = 1;
    down.Highest = 3;
    down.Wraps = false;

    seen.clear();
    current = {};

    for (int i = 0; i < 4; i++)
    {
        current = NextMemoryValue(down, current, {});
        seen.push_back(WholeOf(current));
    }

    VERIFY_IS_TRUE((seen == std::vector<uint32_t>{ 3, 2, 1, 1 }));

    // Set with nothing to set leaves it, and Clear empties it.
    SetMemorySettings set{};
    VERIFY_ARE_EQUAL(1u, WholeOf(NextMemoryValue(set, current, {})));

    SetMemorySettings clear{};
    clear.Action = MemoryAction::Clear;
    VERIFY_IS_FALSE(NextMemoryValue(clear, current, {}).HasValue);
}

void LogicStepTests::PutValuePutsAMemoryIntoTheChannel()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"fc");
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"synth");

    auto& set = AddBlock(patch, L"set", BlockKind::SetMemory);
    set.Settings.SetMemory.Memory = L"Channel";
    set.Settings.SetMemory.EveryMessage = false;
    set.Settings.SetMemory.Trigger.Kind = GateTriggerKind::ProgramChange;
    set.Settings.SetMemory.Value = FromPart(MessagePart::Program);

    auto& put = AddBlock(patch, L"put", BlockKind::PutValue);
    put.Settings.PutValue.Target.Part = MessagePart::Channel;
    put.Settings.PutValue.Value = FromName(LogicSourceKind::Memory, L"Channel");

    Link(patch, L"fc", L"set");
    Link(patch, L"kb", L"put");
    Link(patch, L"put", L"synth");

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    // Empty, so the note goes on as it is.
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 60, 100));
    sink.Arrive(L"dev-fc", Midi1(0, ProgramChange, 0, 3, 0));
    sink.Arrive(L"dev-kb", Midi1(0, NoteOn, 0, 62, 100));

    // A program past 15 goes on the last channel there is.
    sink.Arrive(L"dev-fc", Midi1(0, ProgramChange, 0, 40, 0));
    sink.Arrive(L"dev-kb", Midi2(0, NoteOn, 0, 64, 0, 0x80000000u));

    auto const sent = sink.Take(L"dev-synth");

    VERIFY_ARE_EQUAL(size_t{ 3 }, sent.size());
    VERIFY_IS_TRUE(sent[0] == Midi1(0, NoteOn, 0, 60, 100));
    VERIFY_IS_TRUE(sent[1] == Midi1(0, NoteOn, 3, 62, 100));
    VERIFY_IS_TRUE(sent[2] == Midi2(0, NoteOn, 15, 64, 0, 0x80000000u));
}

void LogicStepTests::PutValueCopiesMidi2ValuesWhole()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");

    auto& tag = AddBlock(patch, L"tag", BlockKind::SetTag);
    tag.Settings.SetTag.Tag = L"Value";
    tag.Settings.SetTag.Value = FromPart(MessagePart::ControllerValue);

    // Zeroed, then put back from the tag.
    auto& zero = AddBlock(patch, L"zero", BlockKind::PutValue);
    zero.Settings.PutValue.Target.Part = MessagePart::ControllerValue;
    zero.Settings.PutValue.Value = FromNumber(0);

    auto& back = AddBlock(patch, L"back", BlockKind::PutValue);
    back.Settings.PutValue.Target.Part = MessagePart::ControllerValue;
    back.Settings.PutValue.Value = FromName(LogicSourceKind::Tag, L"Value");

    Link(patch, L"in", L"tag");
    Link(patch, L"tag", L"zero");
    Link(patch, L"zero", L"back");
    Link(patch, L"back", L"out");

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-in", Midi2(0, ControlChange, 0, 74, 0, 0x12345678u));
    sink.Arrive(L"dev-in", Midi1(0, ControlChange, 0, 74, 99));

    auto const sent = sink.Take(L"dev-out");

    VERIFY_ARE_EQUAL(size_t{ 2 }, sent.size());
    VERIFY_IS_TRUE(sent[0] == Midi2(0, ControlChange, 0, 74, 0, 0x12345678u));
    VERIFY_IS_TRUE(sent[1] == Midi1(0, ControlChange, 0, 74, 99));

    // Between resolutions, the MIDI 2.0 way: the top stays the top.
    VERIFY_ARE_EQUAL(0xFFFFFFFFu, FieldValueOf(ScaledValue(127, SevenBitRange), 32));
    VERIFY_ARE_EQUAL(0x80000000u, FieldValueOf(ScaledValue(64, SevenBitRange), 32));
    VERIFY_ARE_EQUAL(127u, FieldValueOf(ScaledValue(0xFFFFFFFFu, ThirtyTwoBitRange), 7));
    VERIFY_ARE_EQUAL(0x2000u, FieldValueOf(ScaledValue(0x80000000u, ThirtyTwoBitRange), 14));
}

void LogicStepTests::PutValueNeverTurnsANoteOnIntoANoteOff()
{
    PutValueSettings put{};
    put.Target.Part = MessagePart::Velocity;

    auto noteOn = Midi1(0, NoteOn, 0, 60, 100);
    VERIFY_IS_TRUE(WritePart(put.Target, PlainValue(0), noteOn.Words.data(), noteOn.Count));
    VERIFY_IS_TRUE(noteOn == Midi1(0, NoteOn, 0, 60, 1));

    // A note on at velocity zero is a note off, and stays one.
    auto noteOff = Midi1(0, NoteOn, 0, 60, 0);
    VERIFY_IS_FALSE(WritePart(put.Target, PlainValue(100), noteOff.Words.data(), noteOff.Count));
    VERIFY_IS_TRUE(noteOff == Midi1(0, NoteOn, 0, 60, 0));

    // A part the message doesn't have is left alone.
    auto control = Midi1(0, ControlChange, 0, 1, 10);
    VERIFY_IS_FALSE(WritePart(put.Target, PlainValue(100), control.Words.data(), control.Count));
    VERIFY_IS_TRUE(control == Midi1(0, ControlChange, 0, 1, 10));

    // A 14-bit MIDI 1.0 pitch bend, low seven bits first.
    PartPlace bend{};
    bend.Part = MessagePart::PitchBend;

    auto message = Midi1(0, PitchBend, 0, 0, 0);
    VERIFY_IS_TRUE(WritePart(bend, ScaledValue(0x80000000u, ThirtyTwoBitRange), message.Words.data(), message.Count));
    VERIFY_ARE_EQUAL(0x2000u, WholeOf(ReadPart(bend, message.Words.data(), message.Count)) << 7);
    VERIFY_ARE_EQUAL(0x2000u, ReadPart(bend, message.Words.data(), message.Count).Raw);
}

void LogicStepTests::ValuesCompareTheSameInBothProtocols()
{
    auto const patch = VelocitySplit();
    auto const graph = Compile(patch);

    LogicSink sink{ graph };

    // Nearly all the way up, and a quarter of the way, in 16 bits.
    sink.Arrive(L"dev-kb", Midi2(0, NoteOn, 0, 60, 0, 0xF0000000u));
    sink.Arrive(L"dev-kb", Midi2(0, NoteOn, 0, 61, 0, 0x40000000u));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());

    // A MIDI 1.0 value compares exactly at its own 0 to 127.
    for (int32_t velocity = 1; velocity <= 127; velocity++)
    {
        auto const hundredths = static_cast<uint32_t>(HundredthsFromSevenBit(velocity));
        auto const value = ScaledValue(static_cast<uint32_t>(velocity), SevenBitRange);

        VERIFY_IS_TRUE(PassesCondition(Condition(LogicTest::Is, hundredths), LogicUnit::Value, value));
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(velocity), WholeOf(value));
    }
}

void LogicStepTests::ALongMessageStaysInOnePiece()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    // Decides by the first byte of each packet, which differs from packet to packet.
    auto& branch = AddBlock(patch, L"br", BlockKind::Branch);
    branch.Settings.Branch.Subject.Kind = LogicSourceKind::Part;
    branch.Settings.Branch.Subject.Place = PartPlace{ MessagePart::Bits, 0, 14, 8 };
    branch.Settings.Branch.Unit = LogicUnit::Number;
    branch.Settings.Branch.Condition = Condition(LogicTest::Is, 0x41);

    Link(patch, L"in", L"br");
    Link(patch, L"br", L"a", BranchYesWay);
    Link(patch, L"br", L"b", BranchNoWay);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-in", SysEx7Packet(1, 0x41));
    sink.Arrive(L"dev-in", SysEx7Packet(2, 0x10));
    sink.Arrive(L"dev-in", SysEx7Packet(3, 0x20));

    VERIFY_ARE_EQUAL(size_t{ 3 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());

    // Once it has ended, the next message is decided by itself.
    sink.Arrive(L"dev-in", SysEx7Packet(0, 0x10));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());
}

void LogicStepTests::ABypassedSwitchCanSendOnlyItsFirstWay()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");
    AddEndpoint(patch, L"c");

    auto& choice = AddBlock(patch, L"sw", BlockKind::Switch);
    choice.Settings.Switch.Cases = { Case(4, LogicTest::Is, 0), Case(2, LogicTest::Is, 1) };
    choice.Bypassed = true;

    Link(patch, L"in", L"sw");
    Link(patch, L"sw", L"a", 4);
    Link(patch, L"sw", L"b", 2);
    Link(patch, L"sw", L"c", SwitchOtherwiseWay);

    {
        auto const graph = Compile(patch);
        LogicSink sink{ graph };

        sink.Arrive(L"dev-in", Midi1(0, NoteOn, 9, 60, 100));

        VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
        VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-b").size());
        VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-c").size());
    }

    patch.Blocks.front().Settings.Switch.Bypass = BypassWay::FirstWay;

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-in", Midi1(0, NoteOn, 9, 60, 100));

    // The first case in the list, whatever its id.
    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-a").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-b").size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, sink.Take(L"dev-c").size());
}

void LogicStepTests::ASetMemoryStepRunsWithNothingAfterIt()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"fc");

    auto& set = AddBlock(patch, L"set", BlockKind::SetMemory);
    set.Settings.SetMemory.Memory = L"Count";
    set.Settings.SetMemory.Action = MemoryAction::StepUp;
    set.Settings.SetMemory.Lowest = 0;
    set.Settings.SetMemory.Highest = 9;

    Link(patch, L"fc", L"set");

    auto const graph = Compile(patch);

    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.Roots.size());

    LogicSink sink{ graph };

    sink.Arrive(L"dev-fc", Midi1(0, ControlChange, 0, 80, 127));
    sink.Arrive(L"dev-fc", Midi1(0, ControlChange, 0, 80, 0));
    sink.Arrive(L"dev-fc", Midi1(0, ControlChange, 0, 80, 127));

    VERIFY_ARE_EQUAL(2u, WholeOf(sink.MemoryNow(L"k|count")));

    // With no name it changes nothing, so it is left out like any step that leads nowhere.
    patch.Blocks.front().Settings.SetMemory.Memory.clear();
    VERIFY_ARE_EQUAL(size_t{ 0 }, Compile(patch).Roots.size());
}

void LogicStepTests::NamesIgnoreCaseAndStopAtTheCap()
{
    VERIFY_IS_TRUE(LogicNameFrom(L"  Played on  ") == L"Played on");
    VERIFY_IS_TRUE(LogicNameFrom(L"a\tb") == L"a b");
    VERIFY_ARE_EQUAL(MaximumLogicNameLength, LogicNameFrom(std::wstring(100, L'x')).size());
    VERIFY_IS_TRUE(SameLogicName(L"Scene", L"SCENE"));

    PatchDocument patch{};
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"out");
    AddEndpoint(patch, L"other");

    // A patch's names get their slots as it is worked through, so 63 tags on other paths come
    // first, then the 64th, then a reader of a name past the cap.
    for (size_t i = 1; i < MaximumTagsPerPatch; i++)
    {
        auto const id = L"t" + std::to_wstring(i);
        auto& tag = AddBlock(patch, id, BlockKind::SetTag);
        tag.Settings.SetTag.Tag = L"Tag " + std::to_wstring(i);
        tag.Settings.SetTag.Value = FromNumber(1);

        Link(patch, L"in", id);
        Link(patch, id, L"other");
    }

    auto& tagZero = AddBlock(patch, L"t0", BlockKind::SetTag);
    tagZero.Settings.SetTag.Tag = L"Tag 0";
    tagZero.Settings.SetTag.Value = FromNumber(5);

    auto& last = AddBlock(patch, L"last", BlockKind::Branch);
    last.Settings.Branch.Subject = FromName(LogicSourceKind::Tag, L"tag past the cap");
    last.Settings.Branch.Unit = LogicUnit::Number;
    last.Settings.Branch.Condition = Condition(LogicTest::IsEmpty, 0);

    auto& first = AddBlock(patch, L"first", BlockKind::Branch);
    first.Settings.Branch.Subject = FromName(LogicSourceKind::Tag, L"TAG 0");
    first.Settings.Branch.Unit = LogicUnit::Number;
    first.Settings.Branch.Condition = Condition(LogicTest::Is, 5);

    Link(patch, L"in", L"t0");
    Link(patch, L"t0", L"last");
    Link(patch, L"last", L"first", BranchYesWay);
    Link(patch, L"first", L"out", BranchYesWay);

    auto const graph = Compile(patch);
    LogicSink sink{ graph };

    sink.Arrive(L"dev-in", Midi1(0, NoteOn, 0, 60, 100));

    VERIFY_ARE_EQUAL(size_t{ 1 }, sink.Take(L"dev-out").size());
}

void LogicStepTests::LogicSettingsSurviveTheFile()
{
    for (auto const kind : { BlockKind::Branch, BlockKind::Switch, BlockKind::SetTag, BlockKind::SetMemory, BlockKind::PutValue })
    {
        auto settings = DefaultBlockSettings(kind);

        settings.Branch.Subject = FromName(LogicSourceKind::Memory, L"Level");
        settings.Branch.Unit = LogicUnit::Value;
        settings.Branch.Condition.Test = LogicTest::Between;
        settings.Branch.Condition.Lowest = 2500;
        settings.Branch.Condition.Highest = 7539;
        settings.Branch.Condition.Values = { 100, 10000 };
        settings.Branch.Unreadable = UnreadableWay::LastWay;
        settings.Branch.Bypass = BypassWay::FirstWay;
        settings.Branch.Scale = ValueScale::Percent;

        settings.Switch.Subject = FromPart(MessagePart::Program);
        settings.Switch.Unit = LogicUnit::Number;
        settings.Switch.Cases = { Case(3, LogicTest::OneOf, 0), Case(1, LogicTest::Between, 0) };
        settings.Switch.Cases[0].Condition.Values = { 1, 2, 3 };
        settings.Switch.Cases[1].Condition.Lowest = 10;
        settings.Switch.Cases[1].Condition.Highest = 20;
        settings.Switch.Unreadable = UnreadableWay::FirstWay;

        settings.SetTag.Tag = L"Played on";
        settings.SetTag.Value.Kind = LogicSourceKind::Part;
        settings.SetTag.Value.Place = PartPlace{ MessagePart::Bits, 1, 23, 16 };

        settings.SetMemory.Memory = L"Scene";
        settings.SetMemory.EveryMessage = false;
        settings.SetMemory.Trigger.Kind = GateTriggerKind::ControlChange;
        settings.SetMemory.Trigger.Number = 80;
        settings.SetMemory.Action = MemoryAction::Toggle;
        settings.SetMemory.Unit = LogicUnit::Value;
        settings.SetMemory.First = 0;
        settings.SetMemory.Second = 10000;
        settings.SetMemory.PassesTriggers = false;

        settings.PutValue.Target.Part = MessagePart::PitchBend;
        settings.PutValue.Value = FromNumber(5039, LogicUnit::Value);
        settings.PutValue.KeepsOutWhenEmpty = true;

        auto const read = BlockSettingsFromJson(kind, BlockSettingsToJson(kind, settings));

        VERIFY_IS_TRUE(BlockSettingsSignature(kind, read) == BlockSettingsSignature(kind, settings));
        VERIFY_IS_FALSE(BlockChangesNothing(kind, read));
    }

    // A test on a part of the message is in that part's unit, whatever the file says.
    auto settings = DefaultBlockSettings(BlockKind::Branch);
    settings.Branch.Subject = FromPart(MessagePart::Channel);
    settings.Branch.Unit = LogicUnit::Value;

    auto const read = BlockSettingsFromJson(BlockKind::Branch, BlockSettingsToJson(BlockKind::Branch, settings));
    VERIFY_IS_TRUE(read.Branch.Unit == LogicUnit::Channel);

    // A new step of each kind changes nothing.
    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::SetTag, DefaultBlockSettings(BlockKind::SetTag)));
    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::SetMemory, DefaultBlockSettings(BlockKind::SetMemory)));
    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::PutValue, DefaultBlockSettings(BlockKind::PutValue)));
    VERIFY_IS_TRUE(DefaultBlockSettings(BlockKind::Branch).Branch.Condition.Test == LogicTest::Anything);
    VERIFY_IS_TRUE(DefaultBlockSettings(BlockKind::Switch).Switch.Cases.empty());
}

void LogicStepTests::TheFileKeepsTheWayEachLinkLeavesBy()
{
    PatchDocument patch{};
    patch.Name = L"Ways";
    AddEndpoint(patch, L"in");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");
    AddEndpoint(patch, L"c");

    AddBlock(patch, L"br", BlockKind::Branch);

    auto& choice = AddBlock(patch, L"sw", BlockKind::Switch);
    choice.Settings.Switch.Cases = { Case(7, LogicTest::Is, 0), Case(2, LogicTest::Is, 1) };

    Link(patch, L"in", L"br");
    Link(patch, L"br", L"sw", BranchNoWay);
    Link(patch, L"br", L"a", BranchYesWay);
    Link(patch, L"sw", L"b", 7);
    Link(patch, L"sw", L"c", SwitchOtherwiseWay);

    auto const text = WritePatchJson(patch);

    VERIFY_IS_TRUE(text.find(L"\"sourceOutput\":\"no\"") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"sourceOutput\":7") != std::wstring::npos);
    VERIFY_IS_TRUE(text.find(L"\"sourceOutput\":\"otherwise\"") != std::wstring::npos);

    auto const read = ReadPatchJson(text, L"x");

    VERIFY_IS_TRUE(read.has_value());
    VERIFY_ARE_EQUAL(patch.Connections.size(), read->Connections.size());

    for (auto const& link : patch.Connections)
    {
        VERIFY_IS_TRUE(read->HasConnection(link.SourceId, link.SourceGroupIndex, link.DestinationId, link.DestinationGroupIndex));
    }

    // No way at all, a way the step doesn't have, and a word for the other kind.
    std::wstring edited{ text };

    auto replace = [&edited](std::wstring const& from, std::wstring const& to)
        {
            auto const at = edited.find(from);
            VERIFY_IS_TRUE(at != std::wstring::npos);
            edited.replace(at, from.size(), to);
        };

    replace(L"\"sourceOutput\":\"otherwise\"", L"\"nothing\":0");
    replace(L"\"sourceOutput\":7", L"\"sourceOutput\":9");
    replace(L"\"sourceOutput\":\"no\"", L"\"sourceOutput\":\"otherwise\"");

    auto const broken = ReadPatchJson(edited, L"x");

    VERIFY_IS_TRUE(broken.has_value());

    // Only the Branch's Yes is left, and the Switch's link with no way took Anything else.
    VERIFY_IS_TRUE(broken->HasConnection(L"br", BranchYesWay, L"a", AllGroups));
    VERIFY_IS_FALSE(broken->HasConnection(L"sw", 7, L"b", AllGroups));
    VERIFY_IS_FALSE(broken->HasConnection(L"br", BranchNoWay, L"sw", AllGroups));
    VERIFY_IS_TRUE(broken->HasConnection(L"sw", SwitchOtherwiseWay, L"c", AllGroups));
}

void LogicStepTests::AFileFromANewerVersionIsMarked()
{
    PatchDocument patch{};
    patch.Name = L"Newer";
    AddEndpoint(patch, L"in");
    AddBlock(patch, L"tr", BlockKind::Transpose);

    auto const text = WritePatchJson(patch);

    auto const current = ReadPatchJson(text, L"x");
    VERIFY_IS_TRUE(current.has_value());
    VERIFY_IS_FALSE(current->IsFromNewerVersion);

    std::wstring newer{ text };
    auto const version = newer.find(L"\"fileVersion\":2");
    VERIFY_IS_TRUE(version != std::wstring::npos);
    newer.replace(version, 15, L"\"fileVersion\":3");

    auto const fromNewer = ReadPatchJson(newer, L"x");
    VERIFY_IS_TRUE(fromNewer.has_value());
    VERIFY_IS_TRUE(fromNewer->IsFromNewerVersion);

    std::wstring unknownStep{ text };
    auto const type = unknownStep.find(L"\"type\":\"transpose\"");
    VERIFY_IS_TRUE(type != std::wstring::npos);
    unknownStep.replace(type, 18, L"\"type\":\"timeTravel\"");

    auto const fromUnknown = ReadPatchJson(unknownStep, L"x");
    VERIFY_IS_TRUE(fromUnknown.has_value());
    VERIFY_IS_TRUE(fromUnknown->IsFromNewerVersion);
    VERIFY_ARE_EQUAL(size_t{ 0 }, fromUnknown->Blocks.size());
}

void LogicStepTests::AFileWrittenTheWayTheGuideSaysReads()
{
    // The logic examples in the guide for AI agents, the way an agent writes them.
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 2,
        "endpoints": [ { "id": "kb", "displayName": "Keyboard" }, { "id": "a", "displayName": "A" },
                       { "id": "b", "displayName": "B" }, { "id": "fs", "displayName": "Footswitch" } ],
        "blocks": [
            { "id": "br", "type": "branch", "settings": { "subject": { "kind": "part", "part": "velocity" }, "test": "atLeast", "value": 50 } },
            { "id": "sw", "type": "switch", "settings": { "subject": { "kind": "memory", "name": "Synth" }, "unit": "number",
              "cases": [ { "id": 1, "test": "is", "value": 0 }, { "id": 2, "test": "is", "value": 1 } ], "unreadable": "firstCase" } },
            { "id": "set", "type": "setMemory", "settings": { "memory": "Synth", "everyMessage": false,
              "trigger": { "message": "controlChange", "number": 80, "test": "atLeast", "value": 64 },
              "action": "toggle", "first": 0, "second": 1, "passTriggers": false } },
            { "id": "put", "type": "putValue", "settings": { "part": "channel", "source": { "kind": "memory", "name": "Synth" } } }
        ],
        "connections": [
            { "id": "c1", "source": "kb", "destination": "br" },
            { "id": "c2", "source": "br", "sourceOutput": "yes", "destination": "a" },
            { "id": "c3", "source": "br", "sourceOutput": "no", "destination": "sw" },
            { "id": "c4", "source": "sw", "sourceOutput": 1, "destination": "put" },
            { "id": "c5", "source": "sw", "sourceOutput": 2, "destination": "b" },
            { "id": "c6", "source": "put", "destination": "a" },
            { "id": "c7", "source": "fs", "destination": "set" },
            { "id": "c8", "source": "sw", "sourceOutput": "1", "destination": "a" }
        ] })", L"Guide");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_IS_FALSE(patch->IsFromNewerVersion);

    auto const way = [&patch](wchar_t const* id) -> std::optional<int32_t>
        {
            auto const* connection = patch->FindConnection(id);
            return connection == nullptr ? std::nullopt : std::optional<int32_t>{ connection->SourceGroupIndex };
        };

    VERIFY_IS_TRUE(way(L"c2") == BranchYesWay);
    VERIFY_IS_TRUE(way(L"c3") == BranchNoWay);
    VERIFY_IS_TRUE(way(L"c4") == 1);
    VERIFY_IS_TRUE(way(L"c5") == 2);

    // A case id written as text names no way, so that connection is left out.
    VERIFY_IS_FALSE(way(L"c8").has_value());

    auto const* branch = patch->FindBlock(L"br");
    VERIFY_IS_NOT_NULL(branch);
    VERIFY_IS_TRUE(branch->Settings.Branch.Unit == LogicUnit::Value);
    VERIFY_IS_TRUE(branch->Settings.Branch.Condition.Test == LogicTest::AtLeast);
    VERIFY_ARE_EQUAL(uint32_t{ 5000 }, branch->Settings.Branch.Condition.Value);

    auto const* choice = patch->FindBlock(L"sw");
    VERIFY_IS_NOT_NULL(choice);
    VERIFY_ARE_EQUAL(size_t{ 2 }, choice->Settings.Switch.Cases.size());
    VERIFY_IS_TRUE(choice->Settings.Switch.Subject.Kind == LogicSourceKind::Memory);
    VERIFY_IS_TRUE(choice->Settings.Switch.Unit == LogicUnit::Number);
    VERIFY_IS_TRUE(choice->Settings.Switch.Unreadable == UnreadableWay::FirstWay);

    auto const* set = patch->FindBlock(L"set");
    VERIFY_IS_NOT_NULL(set);
    VERIFY_IS_FALSE(set->Settings.SetMemory.EveryMessage);
    VERIFY_IS_TRUE(set->Settings.SetMemory.Trigger.Kind == GateTriggerKind::ControlChange);
    VERIFY_ARE_EQUAL(int16_t{ 80 }, set->Settings.SetMemory.Trigger.Number);
    VERIFY_IS_TRUE(set->Settings.SetMemory.Action == MemoryAction::Toggle);
    VERIFY_IS_FALSE(set->Settings.SetMemory.PassesTriggers);

    auto const* put = patch->FindBlock(L"put");
    VERIFY_IS_NOT_NULL(put);
    VERIFY_IS_TRUE(put->Settings.PutValue.Target.Part == MessagePart::Channel);
    VERIFY_IS_TRUE(put->Settings.PutValue.Value.Kind == LogicSourceKind::Memory);
    VERIFY_IS_TRUE(put->Settings.PutValue.Value.Name == L"Synth");
}

namespace
{
    TraceMessage Traced(Message const& message)
    {
        TraceMessage traced{};
        std::copy_n(message.Words.begin(), message.Count, traced.Words.begin());
        traced.WordCount = message.Count;
        return traced;
    }

    bool Has(std::vector<std::wstring> const& list, std::wstring const& id)
    {
        return std::find(list.begin(), list.end(), id) != list.end();
    }
}

void LogicStepTests::ATraceShowsTheWayEachMessageGoes()
{
    auto const patch = VelocitySplit();

    auto const result = TracePatch(patch, L"kb", {
        Traced(Midi1(0, NoteOn, 0, 60, 120)),
        Traced(Midi1(0, NoteOn, 0, 62, 40)),
        Traced(Midi1(0, NoteOn, 0, 60, 0)) });

    VERIFY_IS_TRUE(result.Outcome == TraceOutcome::Traced);
    VERIFY_ARE_EQUAL(size_t{ 3 }, result.Steps.size());

    // Played hard: in from the keyboard, through the Branch, out Yes to A and nowhere else.
    auto const& hard = result.Steps[0];
    VERIFY_IS_TRUE(hard.Nodes.front() == L"kb");
    VERIFY_IS_TRUE(Has(hard.Nodes, L"br"));
    VERIFY_IS_TRUE(Has(hard.Nodes, L"a"));
    VERIFY_IS_FALSE(Has(hard.Nodes, L"b"));
    VERIFY_IS_TRUE(Has(hard.Links, L"kb>br/-1"));
    VERIFY_IS_TRUE(Has(hard.Links, L"br>a/0"));
    VERIFY_IS_FALSE(Has(hard.Links, L"br>b/1"));
    VERIFY_ARE_EQUAL(size_t{ 1 }, hard.Arrivals.size());
    VERIFY_IS_TRUE(hard.Arrivals[0].EndpointId == L"a");
    VERIFY_ARE_EQUAL(Traced(Midi1(0, NoteOn, 0, 60, 120)).Words[0], hard.Arrivals[0].Message.Words[0]);

    // Played softly: No, to B.
    auto const& soft = result.Steps[1];
    VERIFY_IS_TRUE(Has(soft.Links, L"br>b/1"));
    VERIFY_IS_FALSE(Has(soft.Links, L"br>a/0"));

    // The note off goes where its note on went, which a trace has to show too.
    auto const& off = result.Steps[2];
    VERIFY_ARE_EQUAL(size_t{ 1 }, off.Arrivals.size());
    VERIFY_IS_TRUE(off.Arrivals[0].EndpointId == L"a");

    // Nothing leaves A, so a trace from it has nothing to show.
    VERIFY_IS_TRUE(TracePatch(patch, L"a", { Traced(Midi1(0, NoteOn, 0, 60, 120)) }).Outcome == TraceOutcome::NothingFromSource);
}

void LogicStepTests::ATraceCarriesMemoriesFromOneMessageToTheNext()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");
    AddEndpoint(patch, L"b");

    // The program change picks the scene and is kept out; notes go by scene.
    auto& set = AddBlock(patch, L"set", BlockKind::SetMemory);
    set.Settings.SetMemory.Memory = L"Scene";
    set.Settings.SetMemory.EveryMessage = false;
    set.Settings.SetMemory.Trigger = GateTrigger{ GateTriggerKind::ProgramChange };
    set.Settings.SetMemory.Value = FromPart(MessagePart::Program);
    set.Settings.SetMemory.PassesTriggers = false;

    auto& choice = AddBlock(patch, L"sw", BlockKind::Switch);
    choice.Settings.Switch.Subject = FromName(LogicSourceKind::Memory, L"scene");
    choice.Settings.Switch.Unit = LogicUnit::Number;
    choice.Settings.Switch.Cases = { Case(1, LogicTest::Is, 0), Case(2, LogicTest::Is, 1) };

    Link(patch, L"kb", L"set");
    Link(patch, L"set", L"sw");
    Link(patch, L"sw", L"a", 1);
    Link(patch, L"sw", L"b", 2);

    auto const result = TracePatch(patch, L"kb", {
        Traced(Midi1(0, NoteOn, 0, 60, 100)),
        Traced(Midi1(0, ProgramChange, 0, 1, 0)),
        Traced(Midi1(0, NoteOn, 0, 62, 100)) });

    VERIFY_ARE_EQUAL(size_t{ 3 }, result.Steps.size());

    // Memories start empty, so the first note has nothing to go by.
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Steps[0].Memories.size());
    VERIFY_IS_TRUE(result.Steps[0].Memories[0].first == L"Scene");
    VERIFY_IS_FALSE(result.Steps[0].Memories[0].second.HasValue);

    // The program change sets it, and is kept out on the way.
    VERIFY_IS_TRUE(result.Steps[1].Memories[0].second.HasValue);
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, WholeOf(result.Steps[1].Memories[0].second));
    VERIFY_IS_TRUE(Has(result.Steps[1].KeptOut, L"set"));
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.Steps[1].Arrivals.size());

    // and the note after it goes by the new scene.
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Steps[2].Arrivals.size());
    VERIFY_IS_TRUE(result.Steps[2].Arrivals[0].EndpointId == L"b");
}

void LogicStepTests::ATraceSaysWhichStepKeptAMessageOut()
{
    PatchDocument patch{};
    AddEndpoint(patch, L"kb");
    AddEndpoint(patch, L"a");

    // Channel 1 only.
    auto& filter = AddBlock(patch, L"ch", BlockKind::ChannelFilter);
    filter.Settings.Filter.Channels.fill(false);
    filter.Settings.Filter.Channels[0] = true;

    Link(patch, L"kb", L"ch");
    Link(patch, L"ch", L"a");

    auto const result = TracePatch(patch, L"kb", {
        Traced(Midi1(0, NoteOn, 1, 60, 100)),
        Traced(Midi1(0, NoteOn, 0, 60, 100)) });

    VERIFY_ARE_EQUAL(size_t{ 2 }, result.Steps.size());

    VERIFY_IS_TRUE(Has(result.Steps[0].KeptOut, L"ch"));
    VERIFY_IS_TRUE(Has(result.Steps[0].Nodes, L"ch"));
    VERIFY_IS_FALSE(Has(result.Steps[0].Nodes, L"a"));
    VERIFY_ARE_EQUAL(size_t{ 0 }, result.Steps[0].Arrivals.size());

    VERIFY_ARE_EQUAL(size_t{ 0 }, result.Steps[1].KeptOut.size());
    VERIFY_ARE_EQUAL(size_t{ 1 }, result.Steps[1].Arrivals.size());
}

void LogicStepTests::ATraceMakesTheMessagesItSays()
{
    auto const noteOn = MakeTraceMessage(TraceMessageKind::NoteOn, 2, 3, 60, 100, false);
    VERIFY_IS_TRUE(Traced(Midi1(2, NoteOn, 3, 60, 100)).Words == noteOn.Words);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, noteOn.WordCount);

    auto const bend = MakeTraceMessage(TraceMessageKind::PitchBend, 0, 0, 0, 8192, false);
    VERIFY_IS_TRUE(Traced(Midi1(0, PitchBend, 0, 0x00, 0x40)).Words == bend.Words);

    // A MIDI 2.0 velocity at the middle of the 7-bit range is the middle of the 16-bit range.
    auto const midi2 = MakeTraceMessage(TraceMessageKind::NoteOn, 0, 0, 60, 64, true);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, midi2.WordCount);
    VERIFY_ARE_EQUAL(0x40903C00u, midi2.Words[0]);
    VERIFY_ARE_EQUAL(0x80000000u, midi2.Words[1]);

    auto const program = MakeTraceMessage(TraceMessageKind::ProgramChange, 0, 9, 5, 0, true);
    VERIFY_ARE_EQUAL(0x40C90000u, program.Words[0]);
    VERIFY_ARE_EQUAL(0x05000000u, program.Words[1]);

    auto const words = ReadTraceWords(L"0x20903C64, 0");
    VERIFY_IS_TRUE(words.has_value());
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, words->WordCount);
    VERIFY_ARE_EQUAL(0x20903C64u, words->Words[0]);

    VERIFY_IS_FALSE(ReadTraceWords(L"").has_value());
    VERIFY_IS_FALSE(ReadTraceWords(L"2090ZZ").has_value());
    VERIFY_IS_FALSE(ReadTraceWords(L"1 2 3 4 5").has_value());
    VERIFY_IS_FALSE(ReadTraceWords(L"123456789").has_value());

    // The group a trace runs on replaces the one in the message, except where there is none.
    VERIFY_ARE_EQUAL(0x25933C64u, WithTraceGroup(noteOn, 5).Words[0]);
    VERIFY_ARE_EQUAL(0xF0000000u, WithTraceGroup(*ReadTraceWords(L"F0000000"), 5).Words[0]);
}
