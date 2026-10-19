// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MessageConverterTests.h"
#include "TestMessages.h"

#include "MessageConverter.h"
#include "PatchSerializer.h"
#include "ProcessingBlock.h"
#include "RouteGraph.h"
#include "StatefulBlocks.h"

#include <algorithm>
#include <initializer_list>
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
    constexpr uint8_t ProgramChange = 0xC;
    constexpr uint8_t ChannelPressure = 0xD;
    constexpr uint8_t PitchBend = 0xE;
    constexpr uint8_t Assignable = 0x3;

    // What a converter did with one message.
    struct Result
    {
        bool Matched{ false };
        std::vector<Message> Sent{};
    };

    // One converter step, remembering what it should between messages.
    class Converter
    {
    public:
        explicit Converter(_In_ MessageConverterSettings settings) :
            m_settings(std::move(settings)),
            m_memory(std::make_unique<ConverterMemory>())
        {
        }

        Result Run(_In_ Message const& message)
        {
            ConvertedMessages converted{};
            Result result{};

            result.Matched = ConvertMessage(m_settings, *m_memory, message.Words.data(), message.Count, converted);

            for (uint8_t i = 0; i < converted.Count; i++)
            {
                Message sent{};
                std::copy_n(converted.Words[i].begin(), converted.Counts[i], sent.Words.begin());
                sent.Count = converted.Counts[i];

                result.Sent.push_back(sent);
            }

            return result;
        }

    private:
        MessageConverterSettings m_settings{};
        std::unique_ptr<ConverterMemory> m_memory{};
    };

    // The one message a converter sent instead.
    Message Only(_In_ Result const& result)
    {
        VERIFY_IS_TRUE(result.Matched);
        VERIFY_ARE_EQUAL(size_t{ 1 }, result.Sent.size());

        return result.Sent.front();
    }

    // Matched, and kept out with nothing sent instead.
    bool KeptOut(_In_ Result const& result)
    {
        return result.Matched && result.Sent.empty();
    }

    ConverterRow Row(_In_ ConverterInput from, _In_ uint8_t lowest, _In_ uint8_t highest, _In_ ControllerMessageKind to)
    {
        ConverterRow row{};

        row.From = from;
        row.Lowest = lowest;
        row.Highest = highest;
        row.To = ControllerMessageFor(to);

        return row;
    }

    MessageConverterSettings With(_In_ std::initializer_list<ConverterRow> rows)
    {
        MessageConverterSettings settings{};
        settings.Rows.assign(rows.begin(), rows.end());

        return settings;
    }

    json::JsonObject ToJson(_In_ MessageConverterSettings const& settings)
    {
        json::JsonObject object{};
        MessageConverterToJson(settings, object);

        // Through text, the way a saved patch comes back.
        return json::JsonObject::Parse(object.Stringify());
    }

    // Records what reaches each destination, keeping the step states between messages.
    class Sink : public RouteSink
    {
    public:
        explicit Sink(_In_ RouteGraph const& graph) :
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
        LogicValue Memory(uint32_t) noexcept override { return {}; }
        void ChangeMemory(uint32_t, SetMemorySettings const&, LogicValue const&) noexcept override {}

        void Send(_In_ uint32_t leaf, _In_reads_(wordCount) uint32_t const* words, _In_ uint8_t wordCount) noexcept override
        {
            Message message{};
            std::copy_n(words, wordCount, message.Words.begin());
            message.Count = wordCount;

            Sent.emplace_back(m_graph.Leaves[leaf].DestinationDeviceId, message);
        }

        BlockState* StateOf(_In_ uint32_t state) noexcept override
        {
            return state < m_graph.States.size() ? &m_states[state] : nullptr;
        }

    private:
        RouteGraph const& m_graph;
        std::unique_ptr<BlockState[]> m_states{};
    };
}

void MessageConverterTests::ANoteBecomesAControlChange()
{
    // Note 27 on channel 3 becomes CC 27 at 50 on channel 4, for a DAW that only learns controllers.
    auto row = Row(ConverterInput::Note, 27, 27, ControllerMessageKind::ControlChange);
    row.FromChannel = 2;
    row.SameChannel = false;
    row.To.Channel = 3;
    row.SameValue = false;
    row.To.ValueHundredths = HundredthsFromSevenBit(50);

    Converter converter{ With({ row }) };

    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOn, 2, 27, 100))) == Midi1(0, ControlChange, 3, 27, 50));

    // Letting go sends the release value, 0, for either kind of MIDI 1.0 note off.
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOff, 2, 27, 64))) == Midi1(0, ControlChange, 3, 27, 0));
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOn, 2, 27, 0))) == Midi1(0, ControlChange, 3, 27, 0));

    // Other notes, other channels and other messages go on as they are.
    VERIFY_IS_FALSE(converter.Run(Midi1(0, NoteOn, 2, 28, 100)).Matched);
    VERIFY_IS_FALSE(converter.Run(Midi1(0, NoteOn, 0, 27, 100)).Matched);
    VERIFY_IS_FALSE(converter.Run(Midi1(0, ControlChange, 2, 27, 100)).Matched);
    VERIFY_IS_FALSE(converter.Run(System(0, 0xF8)).Matched);
    VERIFY_IS_FALSE(converter.Run(SysEx7(0)).Matched);

    // As the routing runs it: the control change goes instead of the note.
    auto settings = DefaultBlockSettings(BlockKind::MessageConverter);
    settings.Converter = With({ row });

    VERIFY_IS_TRUE(IsStatefulBlock(BlockKind::MessageConverter));
    VERIFY_IS_FALSE(BlockChangesNothing(BlockKind::MessageConverter, settings));
    VERIFY_IS_TRUE(CategoryOf(BlockKind::MessageConverter) == BlockCategory::Transform);
    VERIFY_IS_TRUE(CanGoIntoConnection(BlockKind::MessageConverter));

    auto state = std::make_unique<BlockState>();
    PrepareBlockState(BlockKind::MessageConverter, *state);

    auto message = Midi1(0, NoteOn, 2, 27, 100);
    StageOutput output{};

    VERIFY_IS_TRUE(RunStatefulBlock(BlockKind::MessageConverter, settings, *state, message.Words.data(), message.Count, 1, output));
    VERIFY_ARE_EQUAL(size_t{ 1 }, output.Count);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, output.Messages[0].Count);
    VERIFY_ARE_EQUAL(Midi1(0, ControlChange, 3, 27, 50).Words[0], output.Messages[0].Words[0]);
    VERIFY_ARE_EQUAL(EveryEdge, output.Messages[0].Edge);

    // A message no row matches goes through unchanged.
    auto other = Midi1(0, NoteOn, 2, 60, 100);
    StageOutput nothing{};

    VERIFY_IS_TRUE(RunStatefulBlock(BlockKind::MessageConverter, settings, *state, other.Words.data(), other.Count, 1, nothing));
    VERIFY_ARE_EQUAL(size_t{ 0 }, nothing.Count);
    VERIFY_IS_TRUE(other == Midi1(0, NoteOn, 2, 60, 100));
}

void MessageConverterTests::LettingGoCanSendNothingOrToggle()
{
    auto row = Row(ConverterInput::Note, 36, 36, ControllerMessageKind::ControlChange);
    row.SameValue = false;
    row.Release = ConverterRelease::Nothing;

    Converter quiet{ With({ row }) };

    VERIFY_IS_TRUE(Only(quiet.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 127));
    VERIFY_IS_TRUE(KeptOut(quiet.Run(Midi1(0, NoteOff, 0, 36, 0))));

    // A note off kept out for good, as the routing runs it.
    auto settings = DefaultBlockSettings(BlockKind::MessageConverter);
    settings.Converter = With({ row });

    auto state = std::make_unique<BlockState>();
    PrepareBlockState(BlockKind::MessageConverter, *state);

    auto off = Midi1(0, NoteOff, 0, 36, 0);
    StageOutput output{};

    VERIFY_IS_FALSE(RunStatefulBlock(BlockKind::MessageConverter, settings, *state, off.Words.data(), off.Count, 1, output));

    // Each press switches between the value and the release value, and letting go sends nothing.
    row.Release = ConverterRelease::Toggle;
    row.To.ReleaseHundredths = HundredthsFromSevenBit(10);

    Converter toggle{ With({ row }) };

    VERIFY_IS_TRUE(Only(toggle.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 127));
    VERIFY_IS_TRUE(KeptOut(toggle.Run(Midi1(0, NoteOff, 0, 36, 0))));
    VERIFY_IS_TRUE(Only(toggle.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 10));
    VERIFY_IS_TRUE(KeptOut(toggle.Run(Midi1(0, NoteOn, 0, 36, 0))));
    VERIFY_IS_TRUE(Only(toggle.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 127));

    // Each pad keeps its own switch.
    row.Highest = 37;

    Converter pads{ With({ row }) };

    VERIFY_IS_TRUE(Only(pads.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 127));
    VERIFY_IS_TRUE(Only(pads.Run(Midi1(0, NoteOn, 0, 37, 90))) == Midi1(0, ControlChange, 0, 37, 127));
    VERIFY_IS_TRUE(Only(pads.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 10));
    VERIFY_IS_TRUE(Only(pads.Run(Midi1(1, NoteOn, 0, 36, 90))) == Midi1(1, ControlChange, 0, 36, 127));

    // A note toggles as a note on, then its note off.
    auto note = Row(ConverterInput::Note, 40, 40, ControllerMessageKind::Note);
    note.Release = ConverterRelease::Toggle;

    Converter latch{ With({ note }) };

    VERIFY_IS_TRUE(Only(latch.Run(Midi1(0, NoteOn, 0, 40, 90))) == Midi1(0, NoteOn, 0, 40, 90));
    VERIFY_IS_TRUE(KeptOut(latch.Run(Midi1(0, NoteOff, 0, 40, 0))));
    VERIFY_IS_TRUE(Only(latch.Run(Midi1(0, NoteOn, 0, 40, 90))) == Midi1(0, NoteOff, 0, 40, 0));
}

void MessageConverterTests::AControlChangeCanBeAButton()
{
    auto row = Row(ConverterInput::ControlChange, 64, 64, ControllerMessageKind::Note);
    row.AsButton = true;
    row.SameNumber = false;
    row.To.Number = 36;

    Converter pedal{ With({ row }) };

    // Pressed at 64 and up, with the value as the velocity.
    VERIFY_IS_TRUE(Only(pedal.Run(Midi1(0, ControlChange, 0, 64, 127))) == Midi1(0, NoteOn, 0, 36, 127));

    // Still down, so nothing more, and the control change itself is kept out.
    VERIFY_IS_TRUE(KeptOut(pedal.Run(Midi1(0, ControlChange, 0, 64, 100))));

    VERIFY_IS_TRUE(Only(pedal.Run(Midi1(0, ControlChange, 0, 64, 63))) == Midi1(0, NoteOff, 0, 36, 0));
    VERIFY_IS_TRUE(KeptOut(pedal.Run(Midi1(0, ControlChange, 0, 64, 0))));

    // Nothing is down to begin with, so letting go first sends nothing.
    Converter fresh{ With({ row }) };

    VERIFY_IS_TRUE(KeptOut(fresh.Run(Midi1(0, ControlChange, 0, 64, 0))));

    // A MIDI 2.0 value is compared on the MIDI 1.0 scale, where its middle is 64.
    VERIFY_IS_TRUE(Only(fresh.Run(Midi2(0, ControlChange, 0, 64, 0, 0x80000000u))) == Midi2(0, NoteOn, 0, 36, 0, 0x80000000u));
    VERIFY_IS_TRUE(Only(fresh.Run(Midi2(0, ControlChange, 0, 64, 0, 0x7FFFFFFFu))) == Midi2(0, NoteOff, 0, 36, 0, 0));

    // The press point is where it's set.
    row.PressHundredths = HundredthsFromSevenBit(100);

    Converter firm{ With({ row }) };

    VERIFY_IS_TRUE(KeptOut(firm.Run(Midi1(0, ControlChange, 0, 64, 99))));
    VERIFY_IS_TRUE(Only(firm.Run(Midi1(0, ControlChange, 0, 64, 100))) == Midi1(0, NoteOn, 0, 36, 100));

    // A button can start and stop a sequencer, on the group the message came in on.
    auto start = Row(ConverterInput::ControlChange, 20, 20, ControllerMessageKind::Start);
    start.AsButton = true;

    Converter transport{ With({ start }) };

    VERIFY_IS_TRUE(Only(transport.Run(Midi1(3, ControlChange, 0, 20, 127))) == System(3, 0xFA));
    VERIFY_IS_TRUE(KeptOut(transport.Run(Midi1(3, ControlChange, 0, 20, 0))));
}

void MessageConverterTests::AMovingValueIsConvertedAsItComes()
{
    auto pressure = Row(ConverterInput::ChannelPressure, 0, 127, ControllerMessageKind::ControlChange);
    pressure.SameNumber = false;
    pressure.To.Number = 1;

    Converter aftertouch{ With({ pressure }) };

    VERIFY_IS_TRUE(Only(aftertouch.Run(Midi1(0, ChannelPressure, 5, 77, 0))) == Midi1(0, ControlChange, 5, 1, 77));
    VERIFY_IS_TRUE(Only(aftertouch.Run(Midi1(0, ChannelPressure, 5, 78, 0))) == Midi1(0, ControlChange, 5, 1, 78));
    VERIFY_IS_TRUE(Only(aftertouch.Run(Midi1(0, ChannelPressure, 5, 0, 0))) == Midi1(0, ControlChange, 5, 1, 0));

    // A MIDI 2.0 value goes over whole.
    VERIFY_IS_TRUE(Only(aftertouch.Run(Midi2(0, ChannelPressure, 5, 0, 0, 0x12345678u))) == Midi2(0, ControlChange, 5, 1, 0, 0x12345678u));

    // A 14-bit bend into a controller, by its top seven bits: the middle stays the middle.
    auto wheel = Row(ConverterInput::PitchBend, 0, 127, ControllerMessageKind::ControlChange);
    wheel.SameNumber = false;
    wheel.To.Number = 2;

    Converter bend{ With({ wheel }) };

    VERIFY_IS_TRUE(Only(bend.Run(Midi1(0, PitchBend, 0, 0, 64))) == Midi1(0, ControlChange, 0, 2, 64));
    VERIFY_IS_TRUE(Only(bend.Run(Midi1(0, PitchBend, 0, 127, 127))) == Midi1(0, ControlChange, 0, 2, 127));

    // And a controller into a bend, the way MIDI 2.0 scales up: 64 is the middle, 127 the top.
    auto fader = Row(ConverterInput::ControlChange, 7, 7, ControllerMessageKind::PitchBend);

    Converter faders{ With({ fader }) };

    VERIFY_IS_TRUE(Only(faders.Run(Midi1(0, ControlChange, 0, 7, 64))) == Midi1(0, PitchBend, 0, 0, 64));
    VERIFY_IS_TRUE(Only(faders.Run(Midi1(0, ControlChange, 0, 7, 127))) == Midi1(0, PitchBend, 0, 127, 127));
    VERIFY_IS_TRUE(Only(faders.Run(Midi1(0, ControlChange, 0, 7, 0))) == Midi1(0, PitchBend, 0, 0, 0));

    // Poly pressure keeps its note when it becomes a note's own controller.
    auto key = Row(ConverterInput::PolyPressure, 0, 127, ControllerMessageKind::PolyPressure);
    key.SameChannel = false;
    key.To.Channel = 9;

    Converter keys{ With({ key }) };

    VERIFY_IS_TRUE(Only(keys.Run(Midi1(0, PolyPressure, 0, 61, 33))) == Midi1(0, PolyPressure, 9, 61, 33));
}

void MessageConverterTests::TheFormatCanStayOrChange()
{
    auto row = Row(ConverterInput::Note, 60, 60, ControllerMessageKind::ControlChange);
    row.SameProtocol = false;
    row.To.Midi1Protocol = false;

    Converter up{ With({ row }) };

    // MIDI 1.0 into MIDI 2.0, scaled the MIDI 2.0 way: 64 is the middle and 127 the top.
    VERIFY_IS_TRUE(Only(up.Run(Midi1(0, NoteOn, 0, 60, 64))) == Midi2(0, ControlChange, 0, 60, 0, 0x80000000u));
    VERIFY_IS_TRUE(Only(up.Run(Midi1(0, NoteOn, 0, 60, 127))) == Midi2(0, ControlChange, 0, 60, 0, 0xFFFFFFFFu));

    row.To.Midi1Protocol = true;

    Converter down{ With({ row }) };

    VERIFY_IS_TRUE(Only(down.Run(Midi2(0, NoteOn, 0, 60, 0, 0xFFFF0000u))) == Midi1(0, ControlChange, 0, 60, 127));

    // A MIDI 2.0 note on at velocity 0 is a quiet note: a press, not a let go.
    VERIFY_IS_TRUE(Only(down.Run(Midi2(0, NoteOn, 0, 60, 0, 0))) == Midi1(0, ControlChange, 0, 60, 0));

    // The same format keeps MIDI 2.0 as MIDI 2.0, the whole velocity carried over.
    row.SameProtocol = true;

    Converter same{ With({ row }) };

    VERIFY_IS_TRUE(Only(same.Run(Midi2(0, NoteOn, 0, 60, 0, 0xFFFF0000u))) == Midi2(0, ControlChange, 0, 60, 0, 0xFFFFFFFFu));
    VERIFY_IS_TRUE(Only(same.Run(Midi1(0, NoteOn, 0, 60, 100))) == Midi1(0, ControlChange, 0, 60, 100));

    // An NRPN always goes as MIDI 2.0.
    auto parameter = Row(ConverterInput::ControlChange, 74, 74, ControllerMessageKind::AssignableController);
    parameter.SameNumber = false;
    parameter.To.Number = (3u << 7) | 16u;

    Converter parameters{ With({ parameter }) };

    VERIFY_IS_TRUE(Only(parameters.Run(Midi1(0, ControlChange, 0, 74, 127))) == Midi2(0, Assignable, 0, 3, 16, 0xFFFFFFFFu));
}

void MessageConverterTests::ANoteFromSomethingThatNeverLetsGoEndsAtOnce()
{
    auto row = Row(ConverterInput::ProgramChange, 0, 127, ControllerMessageKind::Note);

    Converter programs{ With({ row }) };

    // The program is the note, and a program change has no velocity, so the step's own is used.
    auto const played = programs.Run(Midi1(0, ProgramChange, 0, 5, 0));

    VERIFY_ARE_EQUAL(size_t{ 2 }, played.Sent.size());
    VERIFY_IS_TRUE(played.Sent[0] == Midi1(0, NoteOn, 0, 5, 100));
    VERIFY_IS_TRUE(played.Sent[1] == Midi1(0, NoteOff, 0, 5, 0));

    // A MIDI 2.0 program change carries its program in the second word.
    auto const midi2 = programs.Run(Midi2(0, ProgramChange, 0, 0, 0, 9u << 24));

    VERIFY_ARE_EQUAL(size_t{ 2 }, midi2.Sent.size());
    VERIFY_IS_TRUE(midi2.Sent[1] == Midi2(0, NoteOff, 0, 9, 0, 0));

    // The same for a value that moves.
    auto moving = Row(ConverterInput::ControlChange, 1, 1, ControllerMessageKind::Note);

    Converter wheel{ With({ moving }) };

    VERIFY_ARE_EQUAL(size_t{ 2 }, wheel.Run(Midi1(0, ControlChange, 0, 1, 50)).Sent.size());
}

void MessageConverterTests::ProgramChangesStepThroughTheirRange()
{
    auto next = Row(ConverterInput::Note, 36, 36, ControllerMessageKind::ProgramChange);
    next.To.Step = ProgramStep::Next;
    next.To.LowestProgram = 0;
    next.To.HighestProgram = 2;
    next.To.Wraps = true;

    auto previous = next;
    previous.Lowest = 37;
    previous.Highest = 37;
    previous.To.Step = ProgramStep::Previous;

    Converter buttons{ With({ next, previous }) };

    auto const programOf = [](Message const& message) { return static_cast<int>((message.Words[0] >> 8) & 0x7F); };
    auto const press = [&buttons, &programOf](uint8_t channel, uint8_t note)
        {
            return programOf(Only(buttons.Run(Midi1(0, NoteOn, channel, note, 100))));
        };

    VERIFY_ARE_EQUAL(0, press(0, 36));
    VERIFY_ARE_EQUAL(1, press(0, 36));
    VERIFY_ARE_EQUAL(2, press(0, 36));
    VERIFY_ARE_EQUAL(0, press(0, 36));

    // A program change has nothing to send when the button is let go.
    VERIFY_IS_TRUE(KeptOut(buttons.Run(Midi1(0, NoteOff, 0, 36, 0))));

    // Previous shares the place, and goes around the other way.
    VERIFY_ARE_EQUAL(2, press(0, 37));
    VERIFY_ARE_EQUAL(1, press(0, 37));

    // Each channel keeps a place of its own.
    VERIFY_ARE_EQUAL(0, press(1, 36));
}

void MessageConverterTests::TheFirstRowThatMatchesIsUsed()
{
    auto drums = Row(ConverterInput::Note, 36, 39, ControllerMessageKind::ControlChange);
    auto never = Row(ConverterInput::Note, 36, 36, ControllerMessageKind::ProgramChange);

    Converter converter{ With({ drums, never }) };

    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOn, 0, 36, 90))) == Midi1(0, ControlChange, 0, 36, 90));
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOn, 0, 39, 90))) == Midi1(0, ControlChange, 0, 39, 90));
    VERIFY_IS_FALSE(converter.Run(Midi1(0, NoteOn, 0, 40, 90)).Matched);
    VERIFY_IS_FALSE(converter.Run(Midi1(0, NoteOn, 0, 35, 90)).Matched);

    // Ends typed the wrong way round still mean the same notes.
    auto backwards = drums;
    backwards.Lowest = 39;
    backwards.Highest = 36;

    Converter flipped{ With({ backwards }) };

    VERIFY_IS_TRUE(Only(flipped.Run(Midi1(0, NoteOn, 0, 37, 90))) == Midi1(0, ControlChange, 0, 37, 90));

    // With no rows, it changes nothing.
    VERIFY_IS_FALSE(Converter{ MessageConverterSettings{} }.Run(Midi1(0, NoteOn, 0, 36, 90)).Matched);
    VERIFY_IS_TRUE(BlockChangesNothing(BlockKind::MessageConverter, DefaultBlockSettings(BlockKind::MessageConverter)));
}

void MessageConverterTests::AMessageKeepsItsGroup()
{
    auto row = Row(ConverterInput::Note, 60, 60, ControllerMessageKind::ControlChange);

    Converter converter{ With({ row }) };

    VERIFY_IS_TRUE(Only(converter.Run(Midi1(5, NoteOn, 0, 60, 90))) == Midi1(5, ControlChange, 0, 60, 90));

    // A message typed in takes the group of the message it replaces.
    auto typed = Row(ConverterInput::Note, 60, 60, ControllerMessageKind::Words);
    typed.To.WordCount = 1;
    typed.To.Words[0] = 0x10FA0000u;

    Converter start{ With({ typed }) };

    VERIFY_IS_TRUE(Only(start.Run(Midi1(5, NoteOn, 0, 60, 90))) == System(5, 0xFA));

    // Nothing without a group, and nothing that isn't one whole message.
    typed.To.WordCount = 4;
    typed.To.Words = { 0xF0000101u, 0x0000001Fu, 0, 0 };

    Converter stream{ With({ typed }) };

    VERIFY_IS_TRUE(KeptOut(stream.Run(Midi1(5, NoteOn, 0, 60, 90))));

    typed.To.WordCount = 1;
    typed.To.Words = { 0x40903C00u, 0, 0, 0 };

    Converter half{ With({ typed }) };

    VERIFY_IS_TRUE(KeptOut(half.Run(Midi1(5, NoteOn, 0, 60, 90))));
}

void MessageConverterTests::AConverterRunsInARoutedPatch()
{
    PatchDocument patch{};

    for (auto const id : { L"pads", L"daw" })
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        patch.Endpoints.push_back(endpoint);
    }

    // Pads into MIDI 2.0 controllers on channel 4.
    auto row = Row(ConverterInput::Note, 36, 51, ControllerMessageKind::ControlChange);
    row.SameChannel = false;
    row.To.Channel = 3;
    row.SameProtocol = false;
    row.To.Midi1Protocol = false;

    PatchBlock block{};
    block.Id = L"convert";
    block.Kind = BlockKind::MessageConverter;
    block.Settings = DefaultBlockSettings(block.Kind);
    block.Settings.Converter = With({ row });
    patch.Blocks.push_back(block);

    auto const link = [&patch](wchar_t const* id, wchar_t const* from, wchar_t const* to)
        {
            PatchConnection connection{};
            connection.Id = id;
            connection.SourceId = from;
            connection.DestinationId = to;
            patch.Connections.push_back(connection);
        };

    link(L"l1", L"pads", L"convert");
    link(L"l2", L"convert", L"daw");

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

    arrive(Midi1(0, NoteOn, 0, 40, 127));
    arrive(Midi1(0, NoteOn, 0, 60, 100));
    arrive(Midi1(0, NoteOff, 0, 40, 0));

    // A one-word note became a two-word controller on its way through.
    VERIFY_ARE_EQUAL(size_t{ 3 }, sink.Sent.size());
    VERIFY_ARE_EQUAL(std::wstring{ L"dev-daw" }, sink.Sent[0].first);
    VERIFY_IS_TRUE(sink.Sent[0].second == Midi2(0, ControlChange, 3, 40, 0, 0xFFFFFFFFu));
    VERIFY_IS_TRUE(sink.Sent[1].second == Midi1(0, NoteOn, 0, 60, 100));
    VERIFY_IS_TRUE(sink.Sent[2].second == Midi2(0, ControlChange, 3, 40, 0, 0));
}

void MessageConverterTests::ConverterSettingsSurviveTheFile()
{
    MessageConverterSettings settings{};
    settings.Scale = ValueScale::Percent;

    auto note = Row(ConverterInput::Note, 27, 27, ControllerMessageKind::ControlChange);
    note.FromChannel = 2;
    note.SameChannel = false;
    note.To.Channel = 3;
    note.SameValue = false;
    note.To.ValueHundredths = 3937;
    note.Release = ConverterRelease::Toggle;
    note.To.ReleaseHundredths = 1000;

    auto button = Row(ConverterInput::ControlChange, 20, 27, ControllerMessageKind::Note);
    button.AsButton = true;
    button.PressHundredths = 2500;
    button.SameNumber = false;
    button.To.Number = 36;
    button.SameProtocol = false;
    button.To.Midi1Protocol = true;
    button.Release = ConverterRelease::Nothing;

    auto program = Row(ConverterInput::ProgramChange, 0, 127, ControllerMessageKind::ProgramChange);
    program.To.Step = ProgramStep::Previous;
    program.To.LowestProgram = 10;
    program.To.HighestProgram = 20;
    program.To.Wraps = false;

    auto bend = Row(ConverterInput::PitchBend, 0, 127, ControllerMessageKind::AssignableController);
    bend.SameNumber = false;
    bend.To.Number = 400;

    auto typed = Row(ConverterInput::PolyPressure, 60, 72, ControllerMessageKind::Words);
    typed.To.WordCount = 2;
    typed.To.Words = { 0x30000000u, 0, 0, 0 };

    settings.Rows = { note, button, program, bend, typed };

    auto const object = ToJson(settings);
    auto const back = MessageConverterFromJson(object);

    VERIFY_ARE_EQUAL(settings.Rows.size(), back.Rows.size());
    VERIFY_IS_TRUE(MessageConverterSignature(settings) == MessageConverterSignature(back));

    // Only what a row uses is written, so the file reads like what it does.
    auto const first = object.GetNamedArray(L"rows").GetObjectAt(0);
    auto const from = first.GetNamedObject(L"from");
    auto const to = first.GetNamedObject(L"to");

    VERIFY_ARE_EQUAL(27.0, from.GetNamedNumber(L"number"));
    VERIFY_ARE_EQUAL(2.0, from.GetNamedNumber(L"channel"));
    VERIFY_IS_FALSE(from.HasKey(L"lowest"));
    VERIFY_IS_FALSE(to.HasKey(L"number"));
    VERIFY_IS_FALSE(to.HasKey(L"midi1"));
    VERIFY_ARE_EQUAL(39.37, to.GetNamedNumber(L"valuePercent"));
    VERIFY_IS_TRUE(to.GetNamedString(L"release") == L"toggle");

    // Every program, and a program change with no number of its own.
    auto const third = object.GetNamedArray(L"rows").GetObjectAt(2);

    VERIFY_IS_FALSE(third.GetNamedObject(L"from").HasKey(L"number"));
    VERIFY_IS_FALSE(third.GetNamedObject(L"from").HasKey(L"lowest"));
    VERIFY_IS_FALSE(third.GetNamedObject(L"to").HasKey(L"release"));

    // A bend has no number to keep, so the NRPN's is always written.
    VERIFY_ARE_EQUAL(400.0, object.GetNamedArray(L"rows").GetObjectAt(3).GetNamedObject(L"to").GetNamedNumber(L"number"));

    // Through the step's own settings too.
    auto block = DefaultBlockSettings(BlockKind::MessageConverter);
    block.Converter = settings;

    auto const reread = BlockSettingsFromJson(BlockKind::MessageConverter, BlockSettingsToJson(BlockKind::MessageConverter, block));

    VERIFY_IS_TRUE(BlockSettingsSignature(BlockKind::MessageConverter, block) == BlockSettingsSignature(BlockKind::MessageConverter, reread));
}

void MessageConverterTests::BadValuesInTheFileFallBack()
{
    auto const read = MessageConverterFromJson(json::JsonObject::Parse(LR"({
        "valueScale": "loud",
        "rows": [
            { "from": { "message": "sysex", "channel": 16, "number": 200, "lowest": -1, "highest": 300 },
              "to": { "message": "nothing", "channel": -1, "number": 1.5, "valuePercent": 101, "midi1": "yes", "release": "later" } },
            "not a row",
            { "from": { "message": "controlChange", "button": true, "pressPercent": 150 },
              "to": { "message": "rpn", "number": 20000, "midi1": true } }
        ] })"));

    VERIFY_IS_TRUE(read.Scale == ValueScale::SevenBit);
    VERIFY_ARE_EQUAL(size_t{ 2 }, read.Rows.size());

    auto const& first = read.Rows[0];

    VERIFY_IS_TRUE(first.From == ConverterInput::Note);
    VERIFY_ARE_EQUAL(-1, static_cast<int>(first.FromChannel));
    VERIFY_ARE_EQUAL(0, static_cast<int>(first.Lowest));
    VERIFY_ARE_EQUAL(127, static_cast<int>(first.Highest));
    VERIFY_IS_TRUE(first.To.Kind == ControllerMessageKind::ControlChange);
    VERIFY_IS_TRUE(first.SameChannel && first.SameNumber && first.SameValue && first.SameProtocol);
    VERIFY_IS_TRUE(first.Release == ConverterRelease::Send);

    auto const& second = read.Rows[1];

    VERIFY_IS_TRUE(second.From == ConverterInput::ControlChange);
    VERIFY_IS_TRUE(second.AsButton);
    VERIFY_ARE_EQUAL(DefaultConverterPressHundredths, second.PressHundredths);
    VERIFY_IS_TRUE(second.To.Kind == ControllerMessageKind::RegisteredController);
    VERIFY_IS_TRUE(second.SameNumber);

    // An RPN always goes as MIDI 2.0, whatever the file says.
    VERIFY_IS_TRUE(second.SameProtocol);
    VERIFY_IS_FALSE(second.To.Midi1Protocol);

    // At most 64 rows.
    json::JsonArray many{};

    for (int i = 0; i < 70; i++)
    {
        many.Append(json::JsonObject{});
    }

    json::JsonObject capped{};
    capped.SetNamedValue(L"rows", many);

    VERIFY_ARE_EQUAL(MaximumConverterRows, MessageConverterFromJson(capped).Rows.size());
}

void MessageConverterTests::AFileWrittenTheWayTheGuideSaysReads()
{
    // The message converter example in the guide for AI agents, the way an agent writes it.
    auto const patch = ReadPatchJson(LR"({
        "fileVersion": 2,
        "endpoints": [ { "id": "controller", "displayName": "Controller" }, { "id": "daw", "displayName": "DAW" } ],
        "blocks": [
            { "id": "pads-to-daw", "type": "messageConverter", "name": "Pads to DAW buttons", "x": 400, "y": 90, "settings": { "valueScale": "sevenBit", "rows": [ { "from": { "message": "note", "channel": 2, "number": 27 }, "to": { "message": "controlChange", "channel": 3, "valuePercent": 39.37, "release": "send", "releasePercent": 0 } }, { "from": { "message": "controlChange", "number": 64, "button": true }, "to": { "message": "note", "number": 36 } } ] } }
        ],
        "connections": [
            { "id": "c1", "source": "controller", "destination": "pads-to-daw" },
            { "id": "c2", "source": "pads-to-daw", "destination": "daw" }
        ] })", L"Guide");

    VERIFY_IS_TRUE(patch.has_value());
    VERIFY_IS_FALSE(patch->IsFromNewerVersion);
    VERIFY_IS_NOT_NULL(patch->FindConnection(L"c1"));
    VERIFY_IS_NOT_NULL(patch->FindConnection(L"c2"));

    auto const* block = patch->FindBlock(L"pads-to-daw");

    VERIFY_IS_NOT_NULL(block);
    VERIFY_IS_TRUE(block->Kind == BlockKind::MessageConverter);
    VERIFY_ARE_EQUAL(size_t{ 2 }, block->Settings.Converter.Rows.size());

    Converter converter{ block->Settings.Converter };

    // Note 27 on channel 3 is CC 27 at 50 on channel 4, and 0 when it's let go.
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOn, 2, 27, 100))) == Midi1(0, ControlChange, 3, 27, 50));
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, NoteOff, 2, 27, 0))) == Midi1(0, ControlChange, 3, 27, 0));

    // The sustain pedal plays note 36 while it's down, on the channel it came in on.
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, ControlChange, 9, 64, 127))) == Midi1(0, NoteOn, 9, 36, 127));
    VERIFY_IS_TRUE(Only(converter.Run(Midi1(0, ControlChange, 9, 64, 0))) == Midi1(0, NoteOff, 9, 36, 0));
}
