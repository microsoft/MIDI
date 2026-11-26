// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ServiceTimingTests.h"

#include "PlaybackEngine.h"
#include "SessionEngineOutput.h"

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace midi2 = winrt::Windows::Devices::Midi2;

namespace
{
    struct Summary
    {
        double Median{ 0 };
        double Percentile95{ 0 };
        double Maximum{ 0 };
        double Minimum{ 0 };
    };

    Summary Summarize(std::vector<double> values)
    {
        Summary summary{};

        if (values.empty())
        {
            return summary;
        }

        std::sort(values.begin(), values.end());
        summary.Minimum = values.front();
        summary.Median = values[values.size() / 2];
        summary.Percentile95 = values[std::min(values.size() - 1, values.size() * 95 / 100)];
        summary.Maximum = values.back();
        return summary;
    }

    // Microseconds from "due" to "at". Negative is early.
    double SignedMicroseconds(uint64_t due, uint64_t at)
    {
        return at >= due
            ? midi2::MidiClock::ConvertTimestampTicksToMicroseconds(at - due)
            : -midi2::MidiClock::ConvertTimestampTicksToMicroseconds(due - at);
    }

    // A MIDI 2.0 controller nobody else sends on the diagnostics loopback, with the message's
    // number in the value.
    constexpr uint32_t MarkerWord0 = 0x40B05500;
}

void ServiceTimingTests::HowLateScheduledMessagesArrive()
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    catch (...)
    {
        // The test host already chose an apartment.
    }

    if (!midi2::MidiApi::EnsureServiceAvailable())
    {
        Log::Result(TestResults::Skipped, L"The MIDI service isn't available.");
        return;
    }

    auto const session = midi2::MidiSession::Create(L"MIDI Sequencer timing measurement");
    auto const sender = session.CreateEndpointConnection(midi2::Diagnostics::MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto const receiver = session.CreateEndpointConnection(midi2::Diagnostics::MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    struct Arrival
    {
        uint32_t Index{ 0 };
        uint64_t ReceivedAt{ 0 };
    };

    std::mutex lock{};
    std::vector<Arrival> arrivals{};
    std::atomic<size_t> arrivalCount{ 0 };

    auto const token = receiver.MessageReceived([&](auto const&, midi2::MidiMessageReceivedEventArgs const& args)
    {
        auto const now = midi2::MidiClock::Now();

        uint32_t word0{}, word1{}, word2{}, word3{};
        args.FillWords(word0, word1, word2, word3);

        if (word0 == MarkerWord0)
        {
            std::scoped_lock guard{ lock };
            arrivals.push_back(Arrival{ word1, now });
            arrivalCount.store(arrivals.size());
        }
    });

    VERIFY_IS_TRUE(sender.Open());
    VERIFY_IS_TRUE(receiver.Open());

    constexpr uint32_t count = 200;

    Log::Comment(L"look-ahead ms: late by (microseconds) min / median / 95th / max, early arrivals, lost");

    auto measure = [&](int64_t lookAheadMilliseconds, bool burst)
    {
        {
            std::scoped_lock guard{ lock };
            arrivals.clear();
            arrivalCount.store(0);
        }

        std::vector<uint64_t> due(count);

        if (burst)
        {
            // What an engine sweep does: everything due in the next half second, handed over at once.
            auto const now = midi2::MidiClock::Now();

            for (uint32_t i = 0; i < count; ++i)
            {
                due[i] = midi2::MidiClock::OffsetTimestampByMicroseconds(now, 50000 + static_cast<int64_t>(i) * 2500);
                sender.SendSingleMessageWords(due[i], MarkerWord0, i);
            }
        }
        else
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                auto const now = midi2::MidiClock::Now();

                due[i] = lookAheadMilliseconds == 0 ? now : midi2::MidiClock::OffsetTimestampByMilliseconds(now, lookAheadMilliseconds);
                sender.SendSingleMessageWords(
                    lookAheadMilliseconds == 0 ? midi2::MidiClock::TimestampConstantSendImmediately() : due[i],
                    MarkerWord0, i);

                // Uneven spacing, so the sends don't fall into step with any timer.
                ::Sleep(3 + (i % 5));
            }
        }

        auto const deadline = ::GetTickCount64() + 3000 + static_cast<uint64_t>(lookAheadMilliseconds);

        while (arrivalCount.load() < count && ::GetTickCount64() < deadline)
        {
            ::Sleep(10);
        }

        std::vector<double> late{};
        size_t early{ 0 };

        {
            std::scoped_lock guard{ lock };

            for (auto const& arrival : arrivals)
            {
                if (arrival.Index < count)
                {
                    auto const microseconds = SignedMicroseconds(due[arrival.Index], arrival.ReceivedAt);
                    early += microseconds < 0 ? 1 : 0;
                    late.push_back(microseconds);
                }
            }
        }

        auto const summary = Summarize(late);

        Log::Comment(String().Format(L"%s%lld: %.0f / %.0f / %.0f / %.0f, %zu early, %zu lost",
            burst ? L"burst " : L"", lookAheadMilliseconds,
            summary.Minimum, summary.Median, summary.Percentile95, summary.Maximum, early, count - late.size()));

        VERIFY_ARE_EQUAL(size_t{ count }, late.size());
    };

    for (int64_t const lookAhead : { 0, 1, 2, 5, 10, 25, 50, 100 })
    {
        measure(lookAhead, false);
    }

    measure(500, true);

    receiver.MessageReceived(token);
    session.Close();
}

void ServiceTimingTests::HowLateAnEngineThreadWakes()
{
    constexpr int samples = 400;
    constexpr int64_t askMicroseconds = 5000;

    LARGE_INTEGER frequency{};
    ::QueryPerformanceFrequency(&frequency);

    auto nowMicroseconds = [&frequency]()
    {
        LARGE_INTEGER counter{};
        ::QueryPerformanceCounter(&counter);
        return static_cast<double>(counter.QuadPart) * 1000000.0 / static_cast<double>(frequency.QuadPart);
    };

    auto report = [](wchar_t const* name, std::vector<double> const& late)
    {
        auto const summary = Summarize(late);
        Log::Comment(String().Format(L"%s: late by (microseconds) median %.0f, 95th %.0f, max %.0f",
            name, summary.Median, summary.Percentile95, summary.Maximum));
    };

    std::vector<double> late{};
    late.reserve(samples);

    for (int i = 0; i < samples; ++i)
    {
        auto const start = nowMicroseconds();
        ::Sleep(static_cast<DWORD>(askMicroseconds / 1000));
        late.push_back(nowMicroseconds() - start - askMicroseconds);
    }

    report(L"Sleep(5)", late);

    auto const timer = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    VERIFY_IS_NOT_NULL(timer);

    late.clear();

    for (int i = 0; i < samples; ++i)
    {
        LARGE_INTEGER due{};
        due.QuadPart = -askMicroseconds * 10;

        auto const start = nowMicroseconds();
        ::SetWaitableTimerEx(timer, &due, 0, nullptr, nullptr, nullptr, 0);
        ::WaitForSingleObject(timer, INFINITE);
        late.push_back(nowMicroseconds() - start - askMicroseconds);
    }

    ::CloseHandle(timer);

    report(L"High resolution timer, 5 ms", late);

    VERIFY_IS_TRUE(true);
}

void ServiceTimingTests::TheEnginePlaysThroughTheService()
{
    using namespace midisequencer;

    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    catch (...)
    {
    }

    if (!midi2::MidiApi::EnsureServiceAvailable())
    {
        Log::Result(TestResults::Skipped, L"The MIDI service isn't available.");
        return;
    }

    // Sixteen eighth notes at 240 BPM, a note every 125 ms.
    Sequence sequence{};
    sequence.Tempo = { TempoPoint{ 0, 240.0, false } };

    Clip clip{};
    clip.Id = L"c-run";
    clip.Length = TicksPerQuarterNote * 8;
    clip.Loop = false;

    for (int i = 0; i < 16; ++i)
    {
        Note note{};
        note.Tick = i * (TicksPerQuarterNote / 2);
        note.Length = TicksPerQuarterNote / 4;
        note.Number = static_cast<uint8_t>(48 + i);
        note.Velocity = 0xC000;
        clip.Notes.push_back(note);
    }

    Track track{};
    track.Id = L"t-run";
    track.Destination.Endpoint = EndpointRef{ L"Loopback A", L"" };
    track.Timeline = { Placement{ clip.Id, 0, 0 } };

    sequence.Tracks = { track };
    sequence.Clips = { clip };
    NormalizeSequence(sequence);

    auto const session = midi2::MidiSession::Create(L"MIDI Sequencer engine measurement");
    auto const receiver = session.CreateEndpointConnection(midi2::Diagnostics::MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    std::mutex lock{};
    std::vector<double> late{};

    auto const token = receiver.MessageReceived([&](auto const&, midi2::MidiMessageReceivedEventArgs const& args)
    {
        auto const now = midi2::MidiClock::Now();

        // The loopback keeps the time a message was scheduled for, so the difference is the whole
        // trip: engine, service, loopback and back.
        if ((args.PeekFirstWord() & 0xFFF00000) == 0x40900000)
        {
            std::scoped_lock guard{ lock };
            late.push_back(SignedMicroseconds(args.Timestamp(), now));
        }
    });

    VERIFY_IS_TRUE(receiver.Open());

    SessionEngineOutput output{ session };
    output.SetResolver([](EndpointRef const&) { return std::wstring{ midi2::Diagnostics::MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId() }; });
    output.Prepare({ track.Destination.Endpoint });

    PlaybackEngine engine{ output, EngineClock{ []() { return midi2::MidiClock::Now(); }, midi2::MidiClock::TimestampFrequency() } };
    engine.SetSequence(std::make_shared<Sequence const>(sequence));
    engine.StartThread();
    engine.Play(0);

    ::Sleep(2300);

    engine.Stop();
    engine.StopThread();
    ::Sleep(100);

    receiver.MessageReceived(token);
    output.CloseAll();
    session.Close();

    auto const summary = Summarize(late);
    Log::Comment(String().Format(L"%zu notes arrived, late by (microseconds) min %.0f, median %.0f, 95th %.0f, max %.0f; %llu dropped",
        late.size(), summary.Minimum, summary.Median, summary.Percentile95, summary.Maximum, output.DroppedCount()));

    VERIFY_ARE_EQUAL(size_t{ 16 }, late.size());
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, output.DroppedCount());
}
