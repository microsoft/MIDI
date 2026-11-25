// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "MeasurementTests.h"
#include "SequenceTestData.h"

#include "SequenceRender.h"
#include "SequenceSerializer.h"

#include <chrono>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    using Clock = std::chrono::steady_clock;

    double MillisecondsSince(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
}

void MeasurementTests::JsonSizeAndLoadTime()
{
    Log::Comment(L"notes, events, characters, UTF-8 MB (approx), write ms, read ms");

    for (size_t const notes : { size_t{ 10000 }, size_t{ 100000 }, size_t{ 400000 } })
    {
        auto const events = notes / 5;
        auto const sequence = testdata::LargeSequence(notes, events);

        auto start = Clock::now();
        auto const text = WriteSequenceJson(sequence);
        auto const writeMilliseconds = MillisecondsSince(start);

        start = Clock::now();
        Sequence read{};
        auto const result = ReadSequenceJson(text, read);
        auto const readMilliseconds = MillisecondsSince(start);

        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_ARE_EQUAL(notes, read.Clips.front().Notes.size());
        VERIFY_ARE_EQUAL(events, read.Clips.front().Events.size());

        // Everything in these files is ASCII, so a character is a byte in UTF-8.
        Log::Comment(String().Format(L"%zu, %zu, %zu, %.1f, %.0f, %.0f",
            notes, events, text.size(), static_cast<double>(text.size()) / (1024.0 * 1024.0), writeMilliseconds, readMilliseconds));

        if (notes == 100000)
        {
            VERIFY_IS_TRUE(readMilliseconds < 10000.0);
        }
    }
}

void MeasurementTests::RenderingAWindowIsCheap()
{
    auto const sequence = testdata::LargeSequence(100000, 20000);
    auto const& track = sequence.Tracks.front();
    auto const end = TrackEndTick(sequence, track);

    // The engine asks for what's due every few milliseconds. At 120 BPM, 10 ms is about 19 ticks.
    constexpr int64_t window = 19;

    std::vector<RenderedMessage> messages{};
    messages.reserve(256);

    size_t windows{ 0 };
    size_t total{ 0 };
    double slowest{ 0 };

    auto const start = Clock::now();

    for (int64_t from = 0; from < end; from += window)
    {
        messages.clear();

        auto const one = Clock::now();
        RenderTrack(sequence, track, from, from + window, messages);
        slowest = std::max(slowest, MillisecondsSince(one));

        total += messages.size();
        ++windows;
    }

    auto const elapsed = MillisecondsSince(start);

    Log::Comment(String().Format(L"%zu windows, %zu messages, %.3f ms in all, %.4f ms a window on average, %.3f ms the slowest",
        windows, total, elapsed, elapsed / static_cast<double>(windows), slowest));

    // Every note on, note off and controller, exactly once.
    VERIFY_ARE_EQUAL(size_t{ 100000 * 2 + 20000 }, total);
    VERIFY_IS_TRUE(elapsed / static_cast<double>(windows) < 1.0);
}
