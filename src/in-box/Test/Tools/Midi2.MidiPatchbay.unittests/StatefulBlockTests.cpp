// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "StatefulBlockTests.h"
#include "TestMessages.h"

#include "RouteGraph.h"
#include "StatefulBlocks.h"

#include <memory>
#include <string>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOff = 0x8;
    constexpr uint8_t NoteOn = 0x9;
    constexpr uint8_t PolyPressure = 0xA;
    constexpr uint8_t ControlChange = 0xB;
    constexpr uint8_t PitchBend = 0xE;
    constexpr uint8_t Registered = 0x2;
    constexpr uint8_t Assignable = 0x3;
    constexpr uint8_t RelativeRegistered = 0x4;
    constexpr uint8_t RelativeAssignable = 0x5;

    struct Result
    {
        bool Passed{ false };
        Message Changed{};
        StageOutput Output{};

        Message Sent(size_t index) const
        {
            Message message{};
            auto const& out = Output.Messages[index];

            std::copy_n(out.Words.begin(), out.Count, message.Words.begin());
            message.Count = out.Count;

            return message;
        }
    };

    Result Run(BlockKind kind, BlockSettings const& settings, BlockState& state, Message const& message, uint32_t edges = 1)
    {
        Result result{};
        result.Changed = message;
        result.Passed = RunStatefulBlock(kind, settings, state, result.Changed.Words.data(), result.Changed.Count, edges, result.Output);

        return result;
    }

    // Records what reaches each destination, keeping the step states between messages.
    class Sink : public RouteSink
    {
    public:
        explicit Sink(RouteGraph const& graph) :
            m_graph(graph),
            m_states(std::make_unique<BlockState[]>((std::max)(graph.States.size(), size_t{ 1 })))
        {
            for (size_t i = 0; i < graph.States.size(); i++)
            {
                PrepareBlockState(graph.States[i].Kind, m_states[i]);
            }
        }

        std::vector<std::pair<std::wstring, Message>> Sent{};

        void CountLink(uint32_t) noexcept override {}
        void CountBlock(uint32_t, bool) noexcept override {}
        void Throttle(uint32_t, uint32_t const*, uint8_t) noexcept override {}
        void Clock(uint32_t, uint32_t const*, uint8_t) noexcept override {}

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

    private:
        RouteGraph const& m_graph;
        std::unique_ptr<BlockState[]> m_states{};
    };
}

void StatefulBlockTests::AGateOpensAndClosesOnStartAndStop()
{
    auto const settings = DefaultBlockSettings(BlockKind::Gate);
    auto state = std::make_unique<BlockState>();

    // Open to begin with, and the triggers go through.
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 0, 60, 100)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, System(0, 0xFC)).Passed);

    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 0, 62, 100)).Passed);
    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, Midi1(0, ControlChange, 0, 1, 10)).Passed);

    // A note off always gets out, in both protocols, so nothing is left sounding.
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOff, 0, 60, 0)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 0, 60, 0)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, Midi2(0, NoteOff, 0, 60, 0, 0)).Passed);

    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, System(0, 0xFA)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 0, 62, 100)).Passed);
}

void StatefulBlockTests::AGateWithOneTriggerTurnsEachTime()
{
    auto settings = DefaultBlockSettings(BlockKind::Gate);

    GateTrigger pedal{};
    pedal.Kind = GateTriggerKind::NoteOn;
    pedal.Number = 36;

    settings.Gate.Open = pedal;
    settings.Gate.Close = pedal;
    settings.Gate.StartsOpen = false;
    settings.Gate.PassesTriggers = false;

    auto state = std::make_unique<BlockState>();
    auto const other = Midi1(0, ControlChange, 0, 7, 100);

    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, other).Passed);

    // Kept out, and each one turns the gate the other way.
    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 9, 36, 100)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::Gate, settings, *state, other).Passed);

    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 9, 36, 100)).Passed);
    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, other).Passed);

    // A different note is just a note.
    VERIFY_IS_FALSE(Run(BlockKind::Gate, settings, *state, Midi1(0, NoteOn, 9, 38, 100)).Passed);
}

void StatefulBlockTests::AGateComparesMidi2ValuesAtSevenBits()
{
    GateTrigger trigger{};
    trigger.Kind = GateTriggerKind::ControlChange;
    trigger.Number = 64;
    trigger.Channel = 3;
    trigger.Test = GateValueTest::AtLeast;
    trigger.Value = 64;

    auto const halfway = Midi2(0, ControlChange, 3, 64, 0, 0x80000000u);
    auto const justBelow = Midi2(0, ControlChange, 3, 64, 0, 0x7FFFFFFFu);

    VERIFY_IS_TRUE(trigger.Matches(halfway.Words.data(), halfway.Count));
    VERIFY_IS_FALSE(trigger.Matches(justBelow.Words.data(), justBelow.Count));

    auto const midi1 = Midi1(0, ControlChange, 3, 64, 64);
    auto const wrongChannel = Midi1(0, ControlChange, 4, 64, 127);
    auto const wrongController = Midi1(0, ControlChange, 3, 65, 127);

    VERIFY_IS_TRUE(trigger.Matches(midi1.Words.data(), midi1.Count));
    VERIFY_IS_FALSE(trigger.Matches(wrongChannel.Words.data(), wrongChannel.Count));
    VERIFY_IS_FALSE(trigger.Matches(wrongController.Words.data(), wrongController.Count));

    trigger.Test = GateValueTest::Below;

    VERIFY_IS_FALSE(trigger.Matches(halfway.Words.data(), halfway.Count));
    VERIFY_IS_TRUE(trigger.Matches(justBelow.Words.data(), justBelow.Count));
}

void StatefulBlockTests::AGateCanMatchExactWords()
{
    GateTrigger trigger{};
    trigger.Kind = GateTriggerKind::Words;
    trigger.WordCount = 1;
    trigger.Words[0] = 0x20903C40u;

    // The group in the words is ignored; the Group setting decides.
    auto const group5 = Midi1(5, NoteOn, 0, 60, 64);
    auto const otherNote = Midi1(5, NoteOn, 0, 61, 64);

    VERIFY_IS_TRUE(trigger.Matches(group5.Words.data(), group5.Count));
    VERIFY_IS_FALSE(trigger.Matches(otherNote.Words.data(), otherNote.Count));

    trigger.Group = 2;

    VERIFY_IS_FALSE(trigger.Matches(group5.Words.data(), group5.Count));

    // Too short a message can't match more words than it has.
    trigger.Group = -1;
    trigger.WordCount = 2;

    VERIFY_IS_FALSE(trigger.Matches(group5.Words.data(), group5.Count));
}

void StatefulBlockTests::AParameterFilterJudgesMidi2ByItsAddress()
{
    auto settings = DefaultBlockSettings(BlockKind::ParameterFilter);
    settings.ParameterFilter.Action = FilterAction::KeepOut;
    settings.ParameterFilter.Parameters.push_back(ParameterMatch{ ParameterKind::Registered, 0, 0 });

    auto state = std::make_unique<BlockState>();

    VERIFY_IS_FALSE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, Registered, 0, 0, 0, 0x12345678u)).Passed);
    VERIFY_IS_FALSE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, RelativeRegistered, 0, 0, 0, 1)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, Registered, 0, 0, 1, 0)).Passed);
    VERIFY_IS_TRUE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, Assignable, 0, 0, 0, 0)).Passed);

    // Everything else is not this step's business.
    VERIFY_IS_TRUE(Run(BlockKind::ParameterFilter, settings, *state, Midi1(0, NoteOn, 0, 60, 100)).Passed);

    // Let only: any NRPN in bank 1.
    settings.ParameterFilter.Action = FilterAction::LetThrough;
    settings.ParameterFilter.Parameters = { ParameterMatch{ ParameterKind::Assignable, 1, -1 } };

    VERIFY_IS_TRUE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, RelativeAssignable, 0, 1, 99, 0)).Passed);
    VERIFY_IS_FALSE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, Assignable, 0, 2, 99, 0)).Passed);
    VERIFY_IS_FALSE(Run(BlockKind::ParameterFilter, settings, *state, Midi2(0, Registered, 0, 1, 99, 0)).Passed);
}

void StatefulBlockTests::AParameterFilterFollowsMidi1Selection()
{
    auto settings = DefaultBlockSettings(BlockKind::ParameterFilter);
    settings.ParameterFilter.Action = FilterAction::KeepOut;
    settings.ParameterFilter.Parameters.push_back(ParameterMatch{ ParameterKind::Registered, 0, 0 });

    auto state = std::make_unique<BlockState>();
    auto const run = [&](Message const& message) { return Run(BlockKind::ParameterFilter, settings, *state, message).Passed; };

    // Pitch bend range: the selection goes through, the value doesn't.
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 101, 0)));
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 100, 0)));
    VERIFY_IS_FALSE(run(Midi1(0, ControlChange, 0, 6, 12)));
    VERIFY_IS_FALSE(run(Midi1(0, ControlChange, 0, 38, 0)));
    VERIFY_IS_FALSE(run(Midi1(0, ControlChange, 0, 96, 0)));

    // Another channel has its own selection, and has selected nothing yet.
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 1, 6, 12)));

    // Fine tuning is let through.
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 100, 1)));
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 6, 64)));

    // An NRPN with the same numbers is a different parameter.
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 99, 0)));
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 98, 0)));
    VERIFY_IS_TRUE(run(Midi1(0, ControlChange, 0, 6, 64)));
}

void StatefulBlockTests::AParameterTransformMovesMidi2Parameters()
{
    auto settings = DefaultBlockSettings(BlockKind::ParameterTransform);

    ParameterMapRow row{};
    row.From = ParameterMatch{ ParameterKind::Registered, 0, 0 };
    row.ToKind = ParameterKind::Assignable;
    row.ToBank = 1;
    row.ToIndex = 2;
    row.Shape.OutputMaximumHundredths = 5000;

    settings.ParameterTransform.Rows.push_back(row);

    auto state = std::make_unique<BlockState>();

    auto const absolute = Run(BlockKind::ParameterTransform, settings, *state, Midi2(3, Registered, 5, 0, 0, 0xFFFFFFFFu));

    VERIFY_IS_TRUE(absolute.Passed);
    VERIFY_ARE_EQUAL(size_t{ 0 }, absolute.Output.Count);
    VERIFY_ARE_EQUAL(Midi2(3, Assignable, 5, 1, 2, 0).Words[0], absolute.Changed.Words[0]);

    // Full scale in, half scale out, at full resolution.
    VERIFY_IS_TRUE(absolute.Changed.Words[1] >= 0x7FFFFFF0u && absolute.Changed.Words[1] <= 0x80000010u);

    // A relative change moves but keeps its step.
    auto const relative = Run(BlockKind::ParameterTransform, settings, *state, Midi2(3, RelativeRegistered, 5, 0, 0, 0xFFFFFFF0u));

    VERIFY_IS_TRUE(relative.Changed == Midi2(3, RelativeAssignable, 5, 1, 2, 0xFFFFFFF0u));

    // A parameter no row names is left alone.
    auto const untouched = Midi2(3, Registered, 5, 0, 1, 1234);

    VERIFY_IS_TRUE(Run(BlockKind::ParameterTransform, settings, *state, untouched).Changed == untouched);
}

void StatefulBlockTests::AParameterTransformPutsMidi1SelectionRight()
{
    auto settings = DefaultBlockSettings(BlockKind::ParameterTransform);

    ParameterMapRow row{};
    row.From = ParameterMatch{ ParameterKind::Registered, 0, 0 };
    row.ToKind = ParameterKind::Assignable;
    row.ToBank = 1;
    row.ToIndex = 2;
    row.Shape.OutputMaximumHundredths = 5000;

    settings.ParameterTransform.Rows.push_back(row);

    auto state = std::make_unique<BlockState>();
    auto const run = [&](Message const& message) { return Run(BlockKind::ParameterTransform, settings, *state, message); };

    // The first half goes as it is, and the second half sends the new selection instead.
    auto const msb = run(Midi1(2, ControlChange, 4, 101, 0));

    VERIFY_IS_TRUE(msb.Passed);
    VERIFY_ARE_EQUAL(size_t{ 0 }, msb.Output.Count);

    auto const lsb = run(Midi1(2, ControlChange, 4, 100, 0));

    VERIFY_IS_TRUE(lsb.Passed);
    VERIFY_ARE_EQUAL(size_t{ 2 }, lsb.Output.Count);
    VERIFY_IS_TRUE(lsb.Sent(0) == Midi1(2, ControlChange, 4, 99, 1));
    VERIFY_IS_TRUE(lsb.Sent(1) == Midi1(2, ControlChange, 4, 98, 2));

    // The coarse value is reshaped on its own.
    auto const coarse = run(Midi1(2, ControlChange, 4, 6, 127));

    VERIFY_IS_TRUE(coarse.Passed);
    VERIFY_IS_TRUE(coarse.Changed == Midi1(2, ControlChange, 4, 6, 63));

    // The fine value sends both halves of the 14 bit result.
    auto const fine = run(Midi1(2, ControlChange, 4, 38, 127));

    VERIFY_ARE_EQUAL(size_t{ 2 }, fine.Output.Count);
    VERIFY_IS_TRUE(fine.Sent(0) == Midi1(2, ControlChange, 4, 6, 64));
    VERIFY_IS_TRUE(fine.Sent(1) == Midi1(2, ControlChange, 4, 38, 0));
}

void StatefulBlockTests::ADistributorTakesTurns()
{
    auto const settings = DefaultBlockSettings(BlockKind::NoteDistributor);
    auto state = std::make_unique<BlockState>();

    auto const play = [&](Message const& message) { return Run(BlockKind::NoteDistributor, settings, *state, message, 3); };

    for (int32_t voice = 0; voice < 3; voice++)
    {
        auto const result = play(Midi1(0, NoteOn, 0, static_cast<uint8_t>(60 + voice * 2), 100));

        VERIFY_ARE_EQUAL(size_t{ 1 }, result.Output.Count);
        VERIFY_ARE_EQUAL(voice, result.Output.Messages[0].Edge);
    }

    // A note off goes where its note went.
    auto const off = play(Midi1(0, NoteOff, 0, 62, 0));

    VERIFY_ARE_EQUAL(1, off.Output.Messages[0].Edge);

    // The next free voice from where the turn has got to.
    VERIFY_ARE_EQUAL(1, play(Midi1(0, NoteOn, 0, 65, 100)).Output.Messages[0].Edge);

    // With none free, the voice whose turn it is cuts its note short.
    auto const stolen = play(Midi1(0, NoteOn, 0, 67, 100));

    VERIFY_ARE_EQUAL(size_t{ 2 }, stolen.Output.Count);
    VERIFY_ARE_EQUAL(2, stolen.Output.Messages[0].Edge);
    VERIFY_IS_TRUE(stolen.Sent(0) == Midi1(0, NoteOff, 0, 64, 0));
    VERIFY_ARE_EQUAL(2, stolen.Output.Messages[1].Edge);

    // Its note off was already sent, so the late one goes nowhere.
    VERIFY_IS_FALSE(play(Midi1(0, NoteOff, 0, 64, 0)).Passed);
}

void StatefulBlockTests::ADistributorCanKeepTheHighestNotes()
{
    auto settings = DefaultBlockSettings(BlockKind::NoteDistributor);
    settings.Distributor.Mode = DistributionMode::HighestNotes;

    auto state = std::make_unique<BlockState>();
    auto const play = [&](Message const& message) { return Run(BlockKind::NoteDistributor, settings, *state, message, 2); };

    VERIFY_ARE_EQUAL(0, play(Midi2(0, NoteOn, 0, 60, 0, 0x80000000u)).Output.Messages[0].Edge);
    VERIFY_ARE_EQUAL(1, play(Midi2(0, NoteOn, 0, 64, 0, 0x80000000u)).Output.Messages[0].Edge);

    // Higher than the lowest playing: it takes that voice, and the old note gets a MIDI 2.0 note off.
    auto const higher = play(Midi2(0, NoteOn, 0, 62, 0, 0x80000000u));

    VERIFY_ARE_EQUAL(size_t{ 2 }, higher.Output.Count);
    VERIFY_ARE_EQUAL(0, higher.Output.Messages[0].Edge);
    VERIFY_IS_TRUE(higher.Sent(0) == Midi2(0, NoteOff, 0, 60, 0, 0));

    // Lower than everything playing: left out, and so is its note off.
    VERIFY_IS_FALSE(play(Midi2(0, NoteOn, 0, 55, 0, 0x80000000u)).Passed);
    VERIFY_IS_FALSE(play(Midi2(0, NoteOff, 0, 55, 0, 0)).Passed);
}

void StatefulBlockTests::ADistributorSendsTheRestWhereItBelongs()
{
    auto settings = DefaultBlockSettings(BlockKind::NoteDistributor);
    auto state = std::make_unique<BlockState>();

    auto const play = [&](Message const& message) { return Run(BlockKind::NoteDistributor, settings, *state, message, 4); };

    play(Midi1(0, NoteOn, 0, 60, 100));
    play(Midi1(0, NoteOn, 0, 64, 100));

    // Pressure on a note follows the note.
    auto const pressure = play(Midi1(0, PolyPressure, 0, 64, 50));

    VERIFY_ARE_EQUAL(size_t{ 1 }, pressure.Output.Count);
    VERIFY_ARE_EQUAL(1, pressure.Output.Messages[0].Edge);

    // Pressure on a note nobody is playing goes nowhere.
    VERIFY_IS_FALSE(play(Midi1(0, PolyPressure, 0, 70, 50)).Passed);

    // By default a control change goes to every voice: nothing in the output.
    auto const everyone = play(Midi1(0, ControlChange, 0, 1, 64));

    VERIFY_IS_TRUE(everyone.Passed);
    VERIFY_ARE_EQUAL(size_t{ 0 }, everyone.Output.Count);

    // Asked not to, it goes to the voice that played last.
    settings.Distributor.PitchBendToEveryVoice = false;

    auto const bend = play(Midi1(0, PitchBend, 0, 0, 0x50));

    VERIFY_ARE_EQUAL(size_t{ 1 }, bend.Output.Count);
    VERIFY_ARE_EQUAL(1, bend.Output.Messages[0].Edge);

    // All notes off reaches everyone and frees the voices, so the turn carries on from voice 3.
    VERIFY_ARE_EQUAL(size_t{ 0 }, play(Midi1(0, ControlChange, 0, 123, 0)).Output.Count);
    VERIFY_ARE_EQUAL(2, play(Midi1(0, NoteOn, 0, 72, 100)).Output.Messages[0].Edge);
    VERIFY_IS_FALSE(play(Midi1(0, NoteOff, 0, 60, 0)).Passed);

    // A system message is for every voice.
    VERIFY_ARE_EQUAL(size_t{ 0 }, play(System(0, 0xF8)).Output.Count);
}

void StatefulBlockTests::ADistributorSendsEachNoteToOneDestination()
{
    PatchDocument patch{};

    for (auto const id : { L"keys", L"a", L"b" })
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        patch.Endpoints.push_back(endpoint);
    }

    PatchBlock block{};
    block.Id = L"poly";
    block.Kind = BlockKind::NoteDistributor;
    block.Settings = DefaultBlockSettings(block.Kind);
    patch.Blocks.push_back(block);

    auto const link = [&patch](wchar_t const* id, wchar_t const* from, wchar_t const* to)
        {
            PatchConnection connection{};
            connection.Id = id;
            connection.SourceId = from;
            connection.DestinationId = to;
            patch.Connections.push_back(connection);
        };

    link(L"l1", L"keys", L"poly");
    link(L"l2", L"poly", L"a");
    link(L"l3", L"poly", L"b");

    RoutePatch input{};
    input.Key = L"k";
    input.Patch = &patch;

    for (auto const& endpoint : patch.Endpoints)
    {
        input.DeviceIds[endpoint.Id] = L"dev-" + endpoint.Id;
    }

    auto const graph = CompileRoutes({ input });

    VERIFY_IS_TRUE(graph.Problems.empty());
    VERIFY_ARE_EQUAL(size_t{ 1 }, graph.States.size());

    Sink sink{ graph };

    auto const arrive = [&](Message const& message)
        {
            for (auto const& root : graph.Roots)
            {
                RunRoot(graph, root, message.Words.data(), message.Count, sink);
            }
        };

    arrive(Midi1(0, NoteOn, 0, 60, 100));
    arrive(Midi1(0, NoteOn, 0, 64, 100));
    arrive(Midi1(0, ControlChange, 0, 1, 64));

    VERIFY_ARE_EQUAL(size_t{ 4 }, sink.Sent.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-a" }, sink.Sent[0].first);
    VERIFY_IS_TRUE(sink.Sent[0].second == Midi1(0, NoteOn, 0, 60, 100));
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-b" }, sink.Sent[1].first);
    VERIFY_IS_TRUE(sink.Sent[1].second == Midi1(0, NoteOn, 0, 64, 100));

    // The control change reaches both.
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-a" }, sink.Sent[2].first);
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-b" }, sink.Sent[3].first);
}
