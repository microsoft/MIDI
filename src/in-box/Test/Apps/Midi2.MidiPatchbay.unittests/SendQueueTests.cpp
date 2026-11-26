// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SendQueueTests.h"

#include "SendQueue.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

using namespace midipatchbay;
using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    // 10 MHz, as MIDI timestamps are
    constexpr uint64_t TicksPerSecond{ 10'000'000 };
    constexpr uint64_t Start{ 100 * TicksPerSecond };

    uint32_t Cc(_In_ uint8_t const controller, _In_ uint8_t const value)
    {
        return 0x20B00000u | (static_cast<uint32_t>(controller) << 8) | value;
    }

    uint32_t NoteOn(_In_ uint8_t const note)
    {
        return 0x20900064u | (static_cast<uint32_t>(note) << 8);
    }

    uint32_t Bend(_In_ uint32_t const value)
    {
        return 0x20E00000u | ((value & 0x7F) << 8) | ((value >> 7) & 0x7F);
    }

    // A long sweep with no value repeated, which only pitch bend has room for
    std::vector<uint32_t> BendSweep(_In_ uint32_t const count)
    {
        std::vector<uint32_t> words;

        for (uint32_t i = 0; i < count; i++)
        {
            words.push_back(Bend(i * 16383 / (count - 1)));
        }

        return words;
    }

    struct Sent
    {
        uint64_t Timestamp{ 0 };
        uint32_t Word{ 0 };
    };

    // Takes everything, moving the clock on whenever the speed asks to wait. The first word of
    // each message.
    std::vector<Sent> Drain(_Inout_ SendQueue& queue, _Inout_ uint64_t& now)
    {
        std::vector<Sent> sent;
        uint32_t buffer[64]{};

        for (int guard = 0; guard < 1'000'000; guard++)
        {
            uint64_t timestamp{ 0 };
            uint32_t messageCount{ 0 };
            uint64_t waitTicks{ 0 };

            auto const taken = queue.Take(now, buffer, static_cast<uint32_t>(std::size(buffer)), timestamp, messageCount, waitTicks);

            if (taken > 0)
            {
                for (uint32_t i = 0; i < taken; i += ::WindowsMidiServicesInternal::MidiSendBacklogPruner::UmpWordCount(buffer[i]))
                {
                    sent.push_back(Sent{ timestamp, buffer[i] });
                }

                continue;
            }

            if (waitTicks == 0)
            {
                break;
            }

            now += waitTicks;
        }

        return sent;
    }

    std::vector<uint8_t> CcValues(_In_ std::vector<Sent> const& sent, _In_ uint8_t const controller)
    {
        std::vector<uint8_t> values;

        for (auto const& message : sent)
        {
            if ((message.Word & 0xFFFFFF00u) == Cc(controller, 0))
            {
                values.push_back(static_cast<uint8_t>(message.Word & 0x7F));
            }
        }

        return values;
    }

    // A fader moved from 0 to 127 and back down, one message each
    std::vector<uint8_t> FaderUpAndDown()
    {
        std::vector<uint8_t> values;

        for (int value = 0; value <= 127; value++) values.push_back(static_cast<uint8_t>(value));
        for (int value = 126; value >= 0; value--) values.push_back(static_cast<uint8_t>(value));

        return values;
    }

    void PushAll(_Inout_ SendQueue& queue, _In_ std::vector<uint8_t> const& values, _In_ uint8_t const controller = 74)
    {
        for (auto const value : values)
        {
            auto const word = Cc(controller, value);
            VERIFY_IS_TRUE(queue.Push(Start, &word, 1));
        }
    }
}


void SendQueueTests::ABackedUpSweepIsThinnedAndEndsWhereItShould()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, true);

    auto const values = FaderUpAndDown();
    PushAll(queue, values);

    uint64_t now{ Start };
    auto const sent = Drain(queue, now);
    auto const arrived = CcValues(sent, 74);

    Log::Comment(String().Format(L"%zu of %zu sent, %llu thinned, %.0f ms at wire speed",
        arrived.size(), values.size(), queue.PrunedMessageCount(), static_cast<double>(now - Start) / 10'000.0));

    VERIFY_IS_LESS_THAN(arrived.size(), values.size(), L"a backlog several times what the speed carries in 100 ms is thinned");
    VERIFY_ARE_EQUAL(static_cast<uint64_t>(values.size() - arrived.size()), queue.PrunedMessageCount(), L"every message is either sent or counted as thinned");
    VERIFY_ARE_EQUAL(queue.WaitingMessageCount(), 0ull, L"nothing is left waiting");

    VERIFY_ARE_EQUAL(static_cast<int>(arrived.front()), 0, L"the sweep starts where it started");
    VERIFY_ARE_EQUAL(static_cast<int>(arrived.back()), 0, L"and ends where it ended");
    VERIFY_IS_TRUE(std::find(arrived.begin(), arrived.end(), static_cast<uint8_t>(127)) != arrived.end(), L"and still reaches the top");

    // still a sweep up and back down, in order
    auto const top = std::find(arrived.begin(), arrived.end(), static_cast<uint8_t>(127));
    VERIFY_IS_TRUE(std::is_sorted(arrived.begin(), top + 1), L"up");
    VERIFY_IS_TRUE(std::is_sorted(top, arrived.end(), std::greater<uint8_t>{}), L"then down");
}

void SendQueueTests::AFarBehindSweepKeepsItsFirstStepsAndItsEnds()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, true);

    // 6,000 bytes: twenty times what wire speed carries in 100 ms
    auto const words = BendSweep(2000);
    VERIFY_IS_TRUE(queue.Push(Start, words.data(), static_cast<uint32_t>(words.size())));

    uint64_t now{ Start };
    auto const sent = Drain(queue, now);

    auto const kept = ::WindowsMidiServicesInternal::MidiSendBacklogPruneStepsKeptInEveryRun;

    VERIFY_ARE_EQUAL(sent.size(), static_cast<size_t>(kept + 2), L"the start, the steps that always go, and the end");

    for (size_t i = 0; i + 1 < sent.size(); i++)
    {
        VERIFY_ARE_EQUAL(sent[i].Word, words[i]);
    }

    VERIFY_ARE_EQUAL(sent.back().Word, words.back());
    VERIFY_ARE_EQUAL(queue.PrunedMessageCount(), static_cast<uint64_t>(words.size() - sent.size()));
}

void SendQueueTests::NothingIsThinnedWhenSkippingIsTurnedOff()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, false);

    auto const words = BendSweep(2000);
    VERIFY_IS_TRUE(queue.Push(Start, words.data(), static_cast<uint32_t>(words.size())));

    uint64_t now{ Start };
    auto const sent = Drain(queue, now);

    VERIFY_ARE_EQUAL(sent.size(), words.size(), L"every message goes, however far behind");
    VERIFY_ARE_EQUAL(queue.PrunedMessageCount(), 0ull);
    VERIFY_IS_GREATER_THAN(now - Start, TicksPerSecond, L"at wire speed, which takes more than a second");
}

void SendQueueTests::NothingIsThinnedWithoutASpeed()
{
    SendQueue queue;
    queue.Configure(0, TicksPerSecond, true);

    auto const values = FaderUpAndDown();
    PushAll(queue, values);

    uint64_t now{ Start };
    VERIFY_IS_TRUE(CcValues(Drain(queue, now), 74) == values, L"a queue that only waits for each send to complete sends everything");
    VERIFY_ARE_EQUAL(queue.PrunedMessageCount(), 0ull);
}

void SendQueueTests::NothingIsThinnedWhileTheBacklogIsShort()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, true);

    // 150 bytes, under the 312 that wire speed carries in 100 ms
    std::vector<uint8_t> values;
    for (int value = 0; value < 50; value++) values.push_back(static_cast<uint8_t>(value));

    PushAll(queue, values);

    uint64_t now{ Start };
    VERIFY_IS_TRUE(CcValues(Drain(queue, now), 74) == values, L"everything goes");
    VERIFY_ARE_EQUAL(queue.PrunedMessageCount(), 0ull);
}

void SendQueueTests::NotesAndTheValuesAroundThemAlwaysGo()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, true);

    std::vector<uint32_t> words;
    auto const sweep = BendSweep(300);

    for (size_t i = 0; i < sweep.size(); i++)
    {
        words.push_back(sweep[i]);

        if (i % 50 == 25)
        {
            words.push_back(NoteOn(static_cast<uint8_t>(60 + i / 50)));
        }
    }

    for (auto const word : words)
    {
        VERIFY_IS_TRUE(queue.Push(Start, &word, 1));
    }

    uint64_t now{ Start };
    auto const sent = Drain(queue, now);

    VERIFY_IS_GREATER_THAN(queue.PrunedMessageCount(), 0ull, L"the sweep was thinned");

    for (size_t i = 0; i < words.size(); i++)
    {
        if ((words[i] & 0xFFF00000u) != 0x20900000u)
        {
            continue;
        }

        auto const found = std::find_if(sent.begin(), sent.end(), [&](Sent const& message) { return message.Word == words[i]; });
        VERIFY_IS_TRUE(found != sent.end(), L"the note is sent");

        VERIFY_IS_TRUE(found != sent.begin() && (found - 1)->Word == words[i - 1], L"right after the value that came before it");
        VERIFY_IS_TRUE((found + 1) != sent.end() && (found + 1)->Word == words[i + 1], L"and right before the value that came after it");
    }
}

void SendQueueTests::MessagesKeepTheirTimestamps()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, true);

    // ten to a timestamp, as a source delivers batches
    auto const sweep = BendSweep(400);

    for (size_t i = 0; i < sweep.size(); i++)
    {
        VERIFY_IS_TRUE(queue.Push(Start + i / 10, &sweep[i], 1));
    }

    uint64_t now{ Start };
    auto const sent = Drain(queue, now);

    VERIFY_IS_GREATER_THAN(queue.PrunedMessageCount(), 0ull, L"the sweep was thinned");

    for (auto const& message : sent)
    {
        auto const pushed = std::find(sweep.begin(), sweep.end(), message.Word);
        VERIFY_IS_TRUE(pushed != sweep.end(), L"every message sent was pushed");
        VERIFY_ARE_EQUAL(message.Timestamp, Start + static_cast<uint64_t>(pushed - sweep.begin()) / 10, L"with the timestamp it was pushed with");
    }

    VERIFY_ARE_EQUAL(sent.front().Word, sweep.front(), L"the first value is sent");
    VERIFY_ARE_EQUAL(sent.back().Word, sweep.back(), L"and the last");
}

void SendQueueTests::SystemExclusiveIsNeverThinned()
{
    SendQueue queue;
    queue.Configure(1, TicksPerSecond, true);

    // 300 packets of a long dump, about 2,000 bytes on a cable
    std::vector<uint32_t> words;
    for (uint32_t packet = 0; packet < 300; packet++)
    {
        auto const form = packet == 0 ? 1u : packet == 299 ? 3u : 2u;
        words.push_back(0x30000000u | (form << 20) | (6u << 16) | (packet & 0x7F));
        words.push_back(packet & 0x7F);
    }

    VERIFY_IS_TRUE(queue.Push(Start, words.data(), static_cast<uint32_t>(words.size())));

    uint64_t now{ Start };
    auto const sent = Drain(queue, now);

    VERIFY_ARE_EQUAL(sent.size(), static_cast<size_t>(300), L"every packet goes");
    VERIFY_ARE_EQUAL(queue.PrunedMessageCount(), 0ull);
}
