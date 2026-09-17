// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace SequencingTests;

namespace
{
    // Fuzzing runs are bounded by a case count rather than by a clock, so a run takes the same
    // time on every machine and never becomes the reason a test pass is slow.
    constexpr size_t TruncationSamples = 120;
    constexpr size_t CorruptionSamples = 400;
    constexpr size_t RandomFileSamples = 250;

    MidiSession MakeSession(_In_ wchar_t const* name)
    {
        auto session = MidiSession::Create(name);

        VERIFY_IS_NOT_NULL(session);

        return session;
    }

    MidiEndpointConnection OpenLoopbackA(_In_ MidiSession const& session)
    {
        auto connection = session.CreateEndpointConnection(LoopbackAEndpointId());

        VERIFY_IS_NOT_NULL(connection);
        VERIFY_IS_TRUE(connection.Open());

        return connection;
    }

    // Reading a deliberately broken file must never throw out of the API. Anything it returns is
    // acceptable; crashing, hanging or throwing is not.
    void ExpectSurvives(_In_ std::vector<uint8_t> const& bytes, _In_ wchar_t const* what)
    {
        try
        {
            auto const result = ReadBytes(bytes);

            VERIFY_IS_TRUE(result != nullptr);

            if (result.Succeeded())
            {
                auto const sequence = result.Sequence();

                VERIFY_IS_NOT_NULL(sequence);

                // Whatever came back has to be self consistent enough to be used.
                VERIFY_IS_TRUE(sequence.LowestNoteNumber() <= sequence.HighestNoteNumber());

                std::vector<MidiSequenceNote> notes(16);
                sequence.FillNotesInTickRange(0, sequence.LastTick(), 0, notes);

                std::vector<uint8_t> counts(sequence.Tracks().Size() + 1);
                sequence.FillSoundingNoteCountsAtTick(0, counts);
            }
        }
        catch (...)
        {
            LOG_OUTPUT(L"threw while reading %s", what);
            VERIFY_FAIL(L"Reading a malformed file threw out of the API");
        }
    }
}

void MidiSequenceStressTests::ReadsDenseContentQuickly()
{
    auto const start = std::chrono::steady_clock::now();

    auto const result = ReadTestFile(L"black-midi.mid");

    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    VERIFY_IS_TRUE(result.Succeeded());

    auto const sequence = result.Sequence();

    VERIFY_ARE_EQUAL(uint32_t{ 20000 }, sequence.NoteCount());

    LOG_OUTPUT(L"read %d events and %d notes in %d ms",
        static_cast<int>(sequence.EventCount()),
        static_cast<int>(sequence.NoteCount()),
        static_cast<int>(elapsed));

    // Generous, but it would catch an accidental per-note allocation or an O(n^2) pass.
    VERIFY_IS_LESS_THAN(elapsed, 5000LL);
}

void MidiSequenceStressTests::WindowedReadsStayCheapOnDenseContent()
{
    auto const sequence = ReadTestSequence(L"black-midi.mid");

    // This is the property the whole design rests on: asking for a window costs what is in the
    // window, not what is in the file.
    std::vector<MidiSequenceNote> notes(2048);

    auto const start = std::chrono::steady_clock::now();

    uint32_t totalWritten{ 0 };

    // Two hundred frames worth of windows, which is several seconds of drawing.
    for (uint32_t frame = 0; frame < 200; ++frame)
    {
        auto const startTick = frame * 20;

        totalWritten += sequence.FillNotesInTickRange(startTick, startTick + 500, 0, notes);
    }

    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    LOG_OUTPUT(L"200 windows returned %d notes in %d ms", static_cast<int>(totalWritten), static_cast<int>(elapsed));

    VERIFY_IS_TRUE(totalWritten > 0);
    VERIFY_IS_LESS_THAN(elapsed, 2000LL);
}

void MidiSequenceStressTests::PlaysDenseContentWithoutFalteringOrHanging()
{
    auto session = MakeSession(L"Stress dense");
    auto connection = OpenLoopbackA(session);

    MidiSequencePlayer player{ connection, FirstGroup() };

    auto const start = std::chrono::steady_clock::now();

    player.SetSequenceAsync(ReadTestSequence(L"black-midi.mid")).get();

    auto const prepared = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    LOG_OUTPUT(L"prepared 41250 events in %d ms", static_cast<int>(prepared));

    // Preparing converts the whole file to Universal MIDI Packets, so it is allowed to take a
    // moment, but not an unreasonable one.
    VERIFY_IS_LESS_THAN(prepared, 15000LL);

    player.Play();

    // Roughly four seconds of very dense music. Stop early rather than sit through all of it.
    ::Sleep(1500);

    VERIFY_IS_TRUE(player.Position().Microseconds > 0);

    player.Stop();
    player.Close();

    session.Close();
}

void MidiSequenceStressTests::SurvivesRapidStartAndStop()
{
    auto session = MakeSession(L"Stress start stop");
    auto connection = OpenLoopbackA(session);

    MidiSequencePlayer player{ connection, FirstGroup() };

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    // Each cycle silences what the previous one started. A leak or a double free shows up here.
    for (uint32_t cycle = 0; cycle < 40; ++cycle)
    {
        player.Play();
        ::Sleep(15);
        player.Stop();
    }

    VERIFY_IS_TRUE(player.State() == MidiSequencePlayerState::Stopped);

    player.Close();
    session.Close();
}

void MidiSequenceStressTests::SurvivesSeekingWhilePlaying()
{
    auto session = MakeSession(L"Stress seek");
    auto connection = OpenLoopbackA(session);

    MidiSequencePlayer player{ connection, FirstGroup() };

    auto const sequence = ReadTestSequence(L"multi-track.mid");

    player.SetSequenceAsync(sequence).get();
    player.Play();

    // Seeking re-chases the instrument state each time, which is the expensive path.
    for (uint32_t index = 0; index < 60; ++index)
    {
        player.SeekToMicroseconds((index * 37) % (sequence.DurationMicroseconds() + 1));
        ::Sleep(5);
    }

    player.Stop();
    player.Close();
    session.Close();
}

void MidiSequenceStressTests::SurvivesMuteAndSoloChurnWhilePlaying()
{
    auto session = MakeSession(L"Stress mute solo");
    auto connection = OpenLoopbackA(session);

    MidiSequencePlayer player{ connection, FirstGroup() };

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();
    player.Play();

    // Every one of these silences whatever the track had started, so it exercises the panic path
    // repeatedly while the worker is also scheduling.
    for (uint32_t index = 0; index < 80; ++index)
    {
        player.SetTrackMuted(static_cast<uint16_t>(1 + (index % 3)), (index % 2) == 0);
        player.SoloTrackIndex((index % 5) == 0 ? static_cast<int32_t>(1 + (index % 3)) : -1);
        ::Sleep(3);
    }

    player.SoloTrackIndex(-1);

    player.Stop();
    player.Close();
    session.Close();
}

void MidiSequenceStressTests::SurvivesSequenceSwapsWhilePlaying()
{
    auto session = MakeSession(L"Stress swap");
    auto connection = OpenLoopbackA(session);

    MidiSequencePlayer player{ connection, FirstGroup() };

    auto const first = ReadTestSequence(L"plain-scale.mid");
    auto const second = ReadTestSequence(L"multi-track.mid");

    for (uint32_t index = 0; index < 10; ++index)
    {
        player.SetSequenceAsync((index % 2) == 0 ? first : second).get();
        player.Play();
        ::Sleep(40);
    }

    player.Stop();
    player.Close();
    session.Close();
}

void MidiSequenceStressTests::SurvivesClosingWhilePlaying()
{
    // Closing mid-playback has to silence the instrument and tear the worker down without
    // deadlocking against it.
    for (uint32_t attempt = 0; attempt < 5; ++attempt)
    {
        auto session = MakeSession(L"Stress close");
        auto connection = OpenLoopbackA(session);

        MidiSequencePlayer player{ connection, FirstGroup() };

        player.SetSequenceAsync(ReadTestSequence(L"black-midi.mid")).get();
        player.Play();

        ::Sleep(50);

        player.Close();

        session.Close();
    }
}

void MidiSequenceStressTests::SurvivesManyPlayersInSuccession()
{
    auto session = MakeSession(L"Stress many players");
    auto connection = OpenLoopbackA(session);

    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    for (uint32_t index = 0; index < 25; ++index)
    {
        MidiSequencePlayer player{ connection, FirstGroup() };

        player.SetSequenceAsync(sequence).get();
        player.Play();
        ::Sleep(10);
        player.Close();
    }

    // The borrowed connection has to still be usable after all of that.
    VERIFY_ARE_EQUAL(
        MidiSendMessageResults::Succeeded,
        connection.SendSingleMessageWords(0, 0x20903C64) & MidiSendMessageResults::Succeeded);

    session.Close();
}

void MidiSequenceStressTests::SurvivesEveryTruncationOfAFile()
{
    auto const full = ReadTestFileBytes(L"multi-track.mid");

    VERIFY_IS_TRUE(full.size() > 0);

    // Every prefix of a real file. A reader that trusts a length it has not checked faults here.
    auto const step = full.size() <= TruncationSamples ? size_t{ 1 } : full.size() / TruncationSamples;

    for (size_t length = 0; length <= full.size(); length += step)
    {
        std::vector<uint8_t> prefix{ full.begin(), full.begin() + static_cast<ptrdiff_t>(length) };

        ExpectSurvives(prefix, L"a truncated file");
    }
}

void MidiSequenceStressTests::SurvivesSingleByteCorruption()
{
    auto const full = ReadTestFileBytes(L"with-chords.mid");

    VERIFY_IS_TRUE(full.size() > 0);

    std::mt19937 generator{ 20260917 };
    std::uniform_int_distribution<size_t> position{ 0, full.size() - 1 };
    std::uniform_int_distribution<int> value{ 0, 255 };

    for (size_t sample = 0; sample < CorruptionSamples; ++sample)
    {
        auto corrupted = full;

        corrupted[position(generator)] = static_cast<uint8_t>(value(generator));

        ExpectSurvives(corrupted, L"a corrupted file");
    }
}

void MidiSequenceStressTests::SurvivesRandomBytes()
{
    std::mt19937 generator{ 987654321 };
    std::uniform_int_distribution<int> value{ 0, 255 };
    std::uniform_int_distribution<size_t> length{ 0, 4096 };

    for (size_t sample = 0; sample < RandomFileSamples; ++sample)
    {
        std::vector<uint8_t> bytes(length(generator));

        for (auto& b : bytes)
        {
            b = static_cast<uint8_t>(value(generator));
        }

        // Half the cases get a real header, so the reader gets past the first gate and has to
        // cope with nonsense in the track data rather than rejecting it immediately.
        if ((sample % 2) == 0 && bytes.size() >= 14)
        {
            uint8_t const header[]{ 0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0, 2, 0x01, 0xE0 };

            std::copy(std::begin(header), std::end(header), bytes.begin());
        }

        ExpectSurvives(bytes, L"random bytes");
    }
}

void MidiSequenceStressTests::SurvivesAFileOfOnlyHeaders()
{
    std::vector<uint8_t> bytes{ 0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0xFF, 0xFF, 0x01, 0xE0 };

    // Thousands of empty track chunks against a header claiming 65535 of them.
    for (size_t index = 0; index < 4000; ++index)
    {
        uint8_t const track[]{ 0x4D, 0x54, 0x72, 0x6B, 0, 0, 0, 0 };

        bytes.insert(bytes.end(), std::begin(track), std::end(track));
    }

    ExpectSurvives(bytes, L"a file of empty tracks");
}

void MidiSequenceStressTests::SurvivesDeeplyNestedVariableLengthQuantities()
{
    // A variable length quantity is supposed to be at most four bytes. This one runs on, which is
    // a classic way to walk a reader off the end of a buffer.
    std::vector<uint8_t> bytes{ 0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 0, 0, 1, 0x01, 0xE0 };

    std::vector<uint8_t> track{};

    for (size_t index = 0; index < 64; ++index)
    {
        track.push_back(0xFF);      // continuation bit set, forever
    }

    track.push_back(0x00);
    track.push_back(0x90);
    track.push_back(60);
    track.push_back(100);

    uint8_t const chunk[]{ 0x4D, 0x54, 0x72, 0x6B };

    bytes.insert(bytes.end(), std::begin(chunk), std::end(chunk));

    auto const length = static_cast<uint32_t>(track.size());

    bytes.push_back(static_cast<uint8_t>((length >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(length & 0xFF));

    bytes.insert(bytes.end(), track.begin(), track.end());

    ExpectSurvives(bytes, L"a runaway variable length quantity");
}
