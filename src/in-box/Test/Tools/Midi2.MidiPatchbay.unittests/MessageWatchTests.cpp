// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MessageWatchTests.h"
#include "TestMessages.h"

#include "MessageWatch.h"
#include "PatchTrace.h"
#include "RouteGraph.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace midipatchbay;
using namespace patchbaytests;

namespace
{
    constexpr uint8_t NoteOn = 0x9;

    void AddEndpoint(PatchDocument& patch, std::wstring const& id)
    {
        PatchEndpoint endpoint{};
        endpoint.Id = id;
        endpoint.DisplayName = id;
        patch.Endpoints.push_back(endpoint);
    }

    void Link(PatchDocument& patch, std::wstring const& id, std::wstring const& from, std::wstring const& to, int32_t toGroup)
    {
        PatchConnection link{};
        link.Id = id;
        link.SourceId = from;
        link.DestinationId = to;
        link.DestinationGroupIndex = toGroup;
        patch.Connections.push_back(link);
    }

    // A keyboard into a synth on group 3, and into a second synth as it is.
    PatchDocument TwoSynths()
    {
        PatchDocument patch{};
        AddEndpoint(patch, L"kb");
        AddEndpoint(patch, L"synth");
        AddEndpoint(patch, L"other");
        Link(patch, L"toSynth", L"kb", L"synth", 2);
        Link(patch, L"toOther", L"kb", L"other", AllGroups);
        return patch;
    }

    // Keeps what each link carried, by cell.
    class CarrySink final : public RouteSink
    {
    public:
        std::vector<std::pair<uint32_t, Message>> Carried{};

        void CountLink(uint32_t) noexcept override {}
        void CountBlock(uint32_t, bool) noexcept override {}
        void Send(uint32_t, uint32_t const*, uint8_t) noexcept override {}
        void Throttle(uint32_t, uint32_t const*, uint8_t) noexcept override {}
        BlockState* StateOf(uint32_t) noexcept override { return nullptr; }
        void Clock(uint32_t, uint32_t const*, uint8_t) noexcept override {}
        LogicValue Memory(uint32_t) noexcept override { return {}; }
        void ChangeMemory(uint32_t, SetMemorySettings const&, LogicValue const&) noexcept override {}

        void LinkCarried(uint32_t cell, uint32_t const* words, uint8_t wordCount) noexcept override
        {
            Message message{};
            std::copy_n(words, wordCount, message.Words.begin());
            message.Count = wordCount;

            Carried.emplace_back(cell, message);
        }
    };

    bool PushWord(WatchBuffer& buffer, uint32_t lease, uint16_t link, uint64_t timestamp, uint32_t word)
    {
        return buffer.Push(lease, link, timestamp, &word, 1);
    }
}

void MessageWatchTests::MessagesComeOutInOrderAsTheyWentIn()
{
    WatchBuffer buffer{};
    auto const lease = buffer.Lease();

    uint32_t const two[]{ 0x40903C00u, 0xFFFF0000u };

    VERIFY_IS_TRUE(PushWord(buffer, lease, 0, 100, 0x20903C64u));
    VERIFY_IS_TRUE(buffer.Push(lease, 3, 200, two, 2));

    std::vector<WatchedMessage> out(8);
    VERIFY_ARE_EQUAL(size_t{ 2 }, buffer.Drain(out.data(), out.size()));

    VERIFY_ARE_EQUAL(uint64_t{ 100 }, out[0].Timestamp);
    VERIFY_ARE_EQUAL(0x20903C64u, out[0].Words[0]);
    VERIFY_ARE_EQUAL(uint8_t{ 1 }, out[0].WordCount);
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, out[0].Link);
    VERIFY_IS_FALSE(out[0].FromTest);

    VERIFY_ARE_EQUAL(uint64_t{ 200 }, out[1].Timestamp);
    VERIFY_ARE_EQUAL(0x40903C00u, out[1].Words[0]);
    VERIFY_ARE_EQUAL(0xFFFF0000u, out[1].Words[1]);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, out[1].Words[2]);
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, out[1].WordCount);
    VERIFY_ARE_EQUAL(uint16_t{ 3 }, out[1].Link);

    // Nothing more, and nothing to put in without words.
    VERIFY_ARE_EQUAL(size_t{ 0 }, buffer.Drain(out.data(), out.size()));
    VERIFY_IS_FALSE(buffer.Push(lease, 0, 0, nullptr, 1));
    VERIFY_IS_FALSE(buffer.Push(lease, 0, 0, two, 0));

    // Never more than asked for at once.
    for (uint32_t i = 0; i < 5; i++)
    {
        PushWord(buffer, lease, 0, i, i);
    }

    VERIFY_ARE_EQUAL(size_t{ 3 }, buffer.Drain(out.data(), 3));
    VERIFY_ARE_EQUAL(size_t{ 2 }, buffer.Drain(out.data(), out.size()));
    VERIFY_ARE_EQUAL(uint32_t{ 4 }, out[1].Words[0]);
}

void MessageWatchTests::AFullBufferDropsAndCounts()
{
    WatchBuffer buffer{};
    auto const lease = buffer.Lease();

    for (uint32_t i = 0; i < WatchBufferCapacity; i++)
    {
        VERIFY_IS_TRUE(PushWord(buffer, lease, 0, i, i));
    }

    // Routing never waits for the window: the newest is dropped and counted.
    for (uint32_t i = 0; i < 10; i++)
    {
        VERIFY_IS_FALSE(PushWord(buffer, lease, 0, 0, 0xDEADBEEFu));
    }

    VERIFY_ARE_EQUAL(uint64_t{ 10 }, buffer.Dropped());

    std::vector<WatchedMessage> out(WatchBufferCapacity + 16);
    VERIFY_ARE_EQUAL(static_cast<size_t>(WatchBufferCapacity), buffer.Drain(out.data(), out.size()));

    for (uint32_t i = 0; i < WatchBufferCapacity; i++)
    {
        VERIFY_ARE_EQUAL(i, out[i].Words[0]);
    }

    // Room again once the window has looked, all the way round the ring.
    for (uint32_t round = 0; round < 3; round++)
    {
        for (uint32_t i = 0; i < WatchBufferCapacity; i++)
        {
            VERIFY_IS_TRUE(PushWord(buffer, lease, 0, i, i + round));
        }

        VERIFY_ARE_EQUAL(static_cast<size_t>(WatchBufferCapacity), buffer.Drain(out.data(), out.size()));
        VERIFY_ARE_EQUAL(WatchBufferCapacity - 1 + round, out[WatchBufferCapacity - 1].Words[0]);
    }

    VERIFY_ARE_EQUAL(uint64_t{ 10 }, buffer.Dropped());
}

void MessageWatchTests::ANewLeaseNeverSeesTheLastWindowsMessages()
{
    WatchBuffer buffer{};
    auto const first = buffer.Lease();

    PushWord(buffer, first, 0, 1, 0x11111111u);
    PushWord(buffer, first, 0, 2, 0x22222222u);

    // Filled up under the first lease, so some were dropped.
    for (uint32_t i = 0; i < WatchBufferCapacity; i++)
    {
        PushWord(buffer, first, 0, 3, 0x33333333u);
    }

    VERIFY_IS_TRUE(buffer.Dropped() > 0);

    auto const second = buffer.Lease();
    VERIFY_ARE_NOT_EQUAL(first, second);
    VERIFY_ARE_EQUAL(second, buffer.CurrentLease());
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, buffer.Dropped());

    // A routing thread still on the old graph puts one more in under the old lease: it doesn't
    // count against the new window.
    VERIFY_IS_FALSE(PushWord(buffer, first, 0, 4, 0x44444444u));
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, buffer.Dropped());

    // The stale ones are thrown away as the new window looks, a buffer's worth at a time.
    std::vector<WatchedMessage> out(64);
    size_t seen{ 0 };

    for (int i = 0; i < 4; i++)
    {
        seen += buffer.Drain(out.data(), out.size());
    }

    VERIFY_ARE_EQUAL(size_t{ 0 }, seen);

    VERIFY_IS_TRUE(PushWord(buffer, second, 2, 5, 0x55555555u));
    VERIFY_IS_TRUE(PushWord(buffer, first, 0, 6, 0x66666666u));

    VERIFY_ARE_EQUAL(size_t{ 1 }, buffer.Drain(out.data(), out.size()));
    VERIFY_ARE_EQUAL(0x55555555u, out[0].Words[0]);
    VERIFY_ARE_EQUAL(uint16_t{ 2 }, out[0].Link);
}

void MessageWatchTests::ManyThreadsPutInAtOnceWithNothingLostOrTorn()
{
    constexpr uint32_t Threads = 4;
    constexpr uint32_t PerThread = 50000;

    WatchBuffer buffer{};
    auto const lease = buffer.Lease();

    std::atomic<uint32_t> finished{ 0 };
    std::atomic<uint64_t> accepted{ 0 };
    std::vector<std::thread> producers{};

    for (uint32_t t = 0; t < Threads; t++)
    {
        producers.emplace_back([&buffer, &finished, &accepted, lease, t]()
            {
                for (uint32_t i = 0; i < PerThread; i++)
                {
                    // Each word says which thread and which message, and the next ones repeat it
                    // in other forms, so a message copied half from one and half from another shows.
                    uint32_t const words[]{ (t << 24) | i, ~((t << 24) | i), i, t };

                    if (buffer.Push(lease, static_cast<uint16_t>(t), i, words, 4))
                    {
                        accepted.fetch_add(1);
                    }
                }

                finished.fetch_add(1);
            });
    }

    std::vector<WatchedMessage> out(512);
    std::vector<int64_t> lastFrom(Threads, -1);
    uint64_t received{ 0 };
    bool torn{ false };
    bool outOfOrder{ false };

    auto const take = [&]()
        {
            auto const count = buffer.Drain(out.data(), out.size());

            for (size_t m = 0; m < count; m++)
            {
                auto const& message = out[m];
                auto const thread = message.Link;
                auto const index = message.Words[2];

                if (message.WordCount != 4 || thread >= Threads || message.Words[3] != thread ||
                    message.Words[0] != ((static_cast<uint32_t>(thread) << 24) | index) ||
                    message.Words[1] != ~message.Words[0] || message.Timestamp != index)
                {
                    torn = true;
                    continue;
                }

                // One thread's messages arrive in the order that thread put them in.
                if (static_cast<int64_t>(index) <= lastFrom[thread])
                {
                    outOfOrder = true;
                }

                lastFrom[thread] = index;
            }

            received += count;
            return count;
        };

    while (finished.load() < Threads)
    {
        take();
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    while (take() > 0)
    {
    }

    VERIFY_IS_FALSE(torn);
    VERIFY_IS_FALSE(outOfOrder);
    VERIFY_ARE_EQUAL(accepted.load(), received);
    VERIFY_ARE_EQUAL(static_cast<uint64_t>(Threads) * PerThread, received + buffer.Dropped());
}

void MessageWatchTests::AFanCopiesToEveryWindowWatching()
{
    WatchBuffer first{};
    WatchBuffer second{};

    WatchFan fan{};
    VERIFY_IS_TRUE(fan.Add(WatchTarget{ &first, first.Lease(), 0 }));
    VERIFY_IS_TRUE(fan.Add(WatchTarget{ &second, second.Lease(), 4 }));
    VERIFY_IS_FALSE(fan.Add(WatchTarget{ nullptr, 1, 0 }));

    uint32_t const word{ 0x20B00140u };
    fan.Deliver(77, &word, 1);

    std::vector<WatchedMessage> out(4);

    VERIFY_ARE_EQUAL(size_t{ 1 }, first.Drain(out.data(), out.size()));
    VERIFY_ARE_EQUAL(uint16_t{ 0 }, out[0].Link);
    VERIFY_ARE_EQUAL(uint64_t{ 77 }, out[0].Timestamp);

    VERIFY_ARE_EQUAL(size_t{ 1 }, second.Drain(out.data(), out.size()));
    VERIFY_ARE_EQUAL(uint16_t{ 4 }, out[0].Link);
    VERIFY_ARE_EQUAL(word, out[0].Words[0]);

    // A link takes so many windows and no more.
    WatchFan full{};

    for (size_t i = 0; i < MaximumWatchersPerLink; i++)
    {
        VERIFY_IS_TRUE(full.Add(WatchTarget{ &first, 1, static_cast<uint16_t>(i) }));
    }

    VERIFY_IS_FALSE(full.Add(WatchTarget{ &first, 1, 99 }));
    VERIFY_ARE_EQUAL(MaximumWatchersPerLink, full.Count);
}

void MessageWatchTests::ClockAndActiveSensingAreRecognized()
{
    VERIFY_IS_TRUE(ClassifyWatched(System(0, 0xF8).Words[0]) == WatchedKind::Clock);
    VERIFY_IS_TRUE(ClassifyWatched(System(5, 0xF8).Words[0]) == WatchedKind::Clock);
    VERIFY_IS_TRUE(ClassifyWatched(System(0, 0xFE).Words[0]) == WatchedKind::ActiveSensing);
    VERIFY_IS_TRUE(ClassifyWatched(Utility().Words[0]) == WatchedKind::Clock);

    VERIFY_IS_TRUE(ClassifyWatched(System(0, 0xFA).Words[0]) == WatchedKind::Other);
    VERIFY_IS_TRUE(ClassifyWatched(Midi1(0, NoteOn, 0, 60, 100).Words[0]) == WatchedKind::Other);
    VERIFY_IS_TRUE(ClassifyWatched(Midi2(0, NoteOn, 0, 60, 0, 0xFFFF0000u).Words[0]) == WatchedKind::Other);
    VERIFY_IS_TRUE(ClassifyWatched(Stream().Words[0]) == WatchedKind::Other);

    // A no-op utility message isn't timing.
    VERIFY_IS_TRUE(ClassifyWatched(0x00000000u) == WatchedKind::Other);
}

void MessageWatchTests::WordsAndTimesReadTheWayTheMonitorShowsThem()
{
    VERIFY_ARE_EQUAL(std::wstring{ L"20903C64" }, FormatWatchedWords({ 0x20903C64u, 0, 0, 0 }, 1));
    VERIFY_ARE_EQUAL(std::wstring{ L"40903C00 FFFF0000" }, FormatWatchedWords({ 0x40903C00u, 0xFFFF0000u, 0, 0 }, 2));
    VERIFY_ARE_EQUAL(std::wstring{ L"F0000101 0000001F 00000000 0000000A" }, FormatWatchedWords({ 0xF0000101u, 0x1Fu, 0, 0xAu }, 4));
    VERIFY_ARE_EQUAL(std::wstring{}, FormatWatchedWords({ 1, 2, 3, 4 }, 0));
    VERIFY_ARE_EQUAL(std::wstring{ L"00000001 00000002 00000003 00000004" }, FormatWatchedWords({ 1, 2, 3, 4 }, 9));

    // 10 MHz, as MIDI timestamps are.
    constexpr uint64_t Frequency = 10'000'000;
    constexpr uint64_t Origin = 5'000 * Frequency;

    VERIFY_ARE_EQUAL(0.0, WatchMilliseconds(Origin, Origin, Frequency));
    VERIFY_IS_TRUE(std::abs(WatchMilliseconds(Origin + 4'812'040, Origin, Frequency) - 481.204) < 0.0001);

    // Scheduled ahead shows when it will play; one from before the window opened is negative.
    VERIFY_IS_TRUE(std::abs(WatchMilliseconds(Origin + 3 * Frequency, Origin, Frequency) - 3000.0) < 0.0001);
    VERIFY_IS_TRUE(std::abs(WatchMilliseconds(Origin - Frequency / 2, Origin, Frequency) + 500.0) < 0.0001);

    // No frequency, no time, rather than a division by zero.
    VERIFY_ARE_EQUAL(0.0, WatchMilliseconds(Origin + 1, Origin, 0));

    // Days of timestamps lose nothing to the conversion.
    VERIFY_IS_TRUE(std::abs(WatchMilliseconds(Origin + 86'400 * Frequency + 1, Origin, Frequency) - 86'400'000.0001) < 0.00001);
}

void MessageWatchTests::TheHistoryKeepsTheNewestAndFindsRowsByNumber()
{
    WatchHistory history{ 3 };

    VERIFY_ARE_EQUAL(uint64_t{ 0 }, history.LastSequence());
    VERIFY_IS_NULL(history.Find(1));

    WatchedMessage message{};

    for (uint32_t i = 1; i <= 3; i++)
    {
        message.Words[0] = i;
        VERIFY_ARE_EQUAL(size_t{ 0 }, history.Add(message));
    }

    VERIFY_ARE_EQUAL(size_t{ 3 }, history.Size());

    // The fourth pushes the first out, and the rest keep their numbers.
    message.Words[0] = 4;
    VERIFY_ARE_EQUAL(size_t{ 1 }, history.Add(message));
    VERIFY_ARE_EQUAL(size_t{ 3 }, history.Size());
    VERIFY_ARE_EQUAL(uint64_t{ 4 }, history.LastSequence());

    VERIFY_IS_NULL(history.Find(1));
    VERIFY_IS_NOT_NULL(history.Find(2));
    VERIFY_ARE_EQUAL(2u, history.Find(2)->Message.Words[0]);
    VERIFY_ARE_EQUAL(4u, history.Find(4)->Message.Words[0]);
    VERIFY_IS_NULL(history.Find(5));
    VERIFY_IS_NULL(history.Find(0));

    // Clear empties it, and numbers carry on, so a list never mistakes a new row for an old one.
    history.Clear();
    VERIFY_ARE_EQUAL(size_t{ 0 }, history.Size());
    VERIFY_IS_NULL(history.Find(4));

    VERIFY_ARE_EQUAL(size_t{ 0 }, history.Add(message));
    VERIFY_ARE_EQUAL(uint64_t{ 5 }, history.LastSequence());
    VERIFY_IS_NOT_NULL(history.Find(5));

    // The window's own limit.
    WatchHistory full{};

    for (size_t i = 0; i < MaximumWatchHistory + 20; i++)
    {
        full.Add(message);
    }

    VERIFY_ARE_EQUAL(MaximumWatchHistory, full.Size());
    VERIFY_IS_NULL(full.Find(20));
    VERIFY_IS_NOT_NULL(full.Find(21));
}

void MessageWatchTests::RoutingHandsOverWhatGoesAlongEachLink()
{
    auto const patch = TwoSynths();

    RoutePatch input{};
    input.Key = L"k";
    input.Patch = &patch;

    for (auto const& endpoint : patch.Endpoints)
    {
        input.DeviceIds[endpoint.Id] = L"dev-" + endpoint.Id;
    }

    auto const graph = CompileRoutes({ input });

    VERIFY_IS_TRUE(graph.Problems.empty());

    CarrySink sink{};
    auto const played = Midi1(0, NoteOn, 1, 60, 100);

    for (auto const& root : graph.Roots)
    {
        RunRoot(graph, root, played.Words.data(), played.Count, sink);
    }

    VERIFY_ARE_EQUAL(size_t{ 2 }, sink.Carried.size());

    // The cell names the link the way the windows ask for it.
    for (auto const& [cell, message] : sink.Carried)
    {
        VERIFY_IS_TRUE(cell < graph.Cells.size());

        if (graph.Cells[cell] == L"k|toSynth")
        {
            // As it reaches the synth: on the group the link sends to.
            VERIFY_ARE_EQUAL(Midi1(2, NoteOn, 1, 60, 100).Words[0], message.Words[0]);
        }
        else
        {
            VERIFY_IS_TRUE(graph.Cells[cell] == L"k|toOther");
            VERIFY_ARE_EQUAL(played.Words[0], message.Words[0]);
        }
    }

    // Muted links aren't routed, so nothing goes along them.
    auto muted = patch;
    muted.FindConnection(L"toSynth")->Muted = true;
    input.Patch = &muted;

    auto const mutedGraph = CompileRoutes({ input });
    CarrySink mutedSink{};

    for (auto const& root : mutedGraph.Roots)
    {
        RunRoot(mutedGraph, root, played.Words.data(), played.Count, mutedSink);
    }

    VERIFY_ARE_EQUAL(size_t{ 1 }, mutedSink.Carried.size());
    VERIFY_IS_TRUE(mutedGraph.Cells[mutedSink.Carried[0].first] == L"k|toOther");
}

void MessageWatchTests::ATraceSaysWhatWentAlongEachLink()
{
    auto const patch = TwoSynths();
    auto const played = Midi1(0, NoteOn, 1, 60, 100);

    TraceMessage traced{};
    traced.Words[0] = played.Words[0];
    traced.WordCount = 1;

    auto const result = TracePatch(patch, L"kb", { traced, traced });

    VERIFY_IS_TRUE(result.Outcome == TraceOutcome::Traced);
    VERIFY_ARE_EQUAL(size_t{ 2 }, result.Steps.size());

    for (auto const& step : result.Steps)
    {
        VERIFY_ARE_EQUAL(size_t{ 2 }, step.Carried.size());

        auto const toSynth = std::find_if(step.Carried.begin(), step.Carried.end(),
            [](TraceCarried const& carried) { return carried.LinkId == L"toSynth"; });
        auto const toOther = std::find_if(step.Carried.begin(), step.Carried.end(),
            [](TraceCarried const& carried) { return carried.LinkId == L"toOther"; });

        VERIFY_IS_TRUE(toSynth != step.Carried.end());
        VERIFY_IS_TRUE(toOther != step.Carried.end());

        VERIFY_ARE_EQUAL(Midi1(2, NoteOn, 1, 60, 100).Words[0], toSynth->Message.Words[0]);
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, toSynth->Message.WordCount);
        VERIFY_ARE_EQUAL(played.Words[0], toOther->Message.Words[0]);
    }
}
