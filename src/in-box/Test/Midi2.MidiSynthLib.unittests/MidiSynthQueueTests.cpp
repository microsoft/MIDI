// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "MidiSynthQueueTests.h"

#include <MidiSynth/SpscRingBuffer.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace MidiSynth;

namespace
{
    struct Packet
    {
        uint32_t Message{ 0 };
        uint32_t Index{ 0 };
        uint32_t Count{ 0 };
    };
}

void MidiSynthQueueTests::TestPushAllIsAllOrNothing()
{
    // Eight slots hold seven items: one is always left empty to tell full from empty.
    SpscRingBuffer<uint32_t, 8> ring;

    const uint32_t first[5]{ 1, 2, 3, 4, 5 };
    const uint32_t tooMany[3]{ 6, 7, 8 };

    VERIFY_IS_TRUE(ring.TryPushAll(first, 5));
    VERIFY_IS_FALSE(ring.TryPushAll(tooMany, 3), L"three items do not fit in two free slots");

    uint32_t value{ 0 };

    for (uint32_t expected = 1; expected <= 5; expected++)
    {
        VERIFY_IS_TRUE(ring.TryPop(value));
        VERIFY_ARE_EQUAL(expected, value);
    }

    VERIFY_IS_FALSE(ring.TryPop(value), L"the refused push left nothing behind");

    // Starts at slot five, so this one wraps.
    const uint32_t full[7]{ 10, 11, 12, 13, 14, 15, 16 };

    VERIFY_IS_TRUE(ring.TryPushAll(full, 7));
    VERIFY_IS_FALSE(ring.TryPushAll(first, 1), L"the ring is full");
    VERIFY_IS_TRUE(ring.TryPushAll(first, 0), L"nothing always fits");

    for (uint32_t expected = 10; expected <= 16; expected++)
    {
        VERIFY_IS_TRUE(ring.TryPop(value));
        VERIFY_ARE_EQUAL(expected, value);
    }

    VERIFY_IS_FALSE(ring.TryPop(value));
}

void MidiSynthQueueTests::TestPushAllNeverShowsPartOfAMessage()
{
    // The synth's worker sends its own replies whenever it finds the dispatcher's queue empty. If
    // that could happen part way through a dispatcher reply, the two would interleave on the wire.
    auto ring = std::make_unique<SpscRingBuffer<Packet, 256>>();

    std::atomic<bool> stop{ false };
    uint64_t pushedMessages{ 0 };

    std::thread producer([&]()
    {
        Packet packets[12]{};
        uint32_t message{ 0 };

        while (!stop.load(std::memory_order_relaxed))
        {
            const uint32_t count = 1 + (message % 12);

            for (uint32_t i = 0; i < count; i++)
            {
                packets[i] = Packet{ message, i, count };
            }

            // A full ring refuses the whole message, which is allowed. Part of one is not.
            if (ring->TryPushAll(packets, count))
            {
                pushedMessages++;
            }
            else
            {
                std::this_thread::yield();
            }

            message++;
        }
    });

    uint64_t messages{ 0 };
    uint64_t emptyPartWay{ 0 };
    uint64_t outOfSequence{ 0 };
    bool open{ false };
    uint32_t expectedIndex{ 0 };
    uint32_t lastMessage{ 0 };

    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);

    while (std::chrono::steady_clock::now() < end)
    {
        Packet packet{};

        if (!ring->TryPop(packet))
        {
            if (open)
            {
                emptyPartWay++;
            }

            continue;
        }

        if (packet.Index != expectedIndex || (packet.Index == 0 && messages > 0 && packet.Message <= lastMessage))
        {
            outOfSequence++;
        }

        open = (packet.Index + 1 < packet.Count);
        expectedIndex = open ? packet.Index + 1 : 0;

        if (!open)
        {
            lastMessage = packet.Message;
            messages++;
        }
    }

    stop.store(true, std::memory_order_relaxed);
    producer.join();

    Log::Comment(String().Format(L"%llu messages pushed, %llu read whole", pushedMessages, messages));

    VERIFY_IS_GREATER_THAN(messages, 1000ull, L"enough traffic to catch a partial message");
    VERIFY_ARE_EQUAL(0ull, emptyPartWay, L"the ring was never empty part way through a message");
    VERIFY_ARE_EQUAL(0ull, outOfSequence, L"every message arrived whole and in order");
}
