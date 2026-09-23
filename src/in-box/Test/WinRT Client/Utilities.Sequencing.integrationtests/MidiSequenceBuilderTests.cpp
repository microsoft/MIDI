// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"
#include "MidiSequenceBuilderTests.h"

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing;

namespace
{
    constexpr uint32_t PollIntervalMilliseconds = 20;

    // Sends on loopback A and listens on loopback B, the same way the player tests do.
    struct BuilderCapture
    {
        MidiSession Session{ nullptr };
        MidiEndpointConnection Sender{ nullptr };
        MidiEndpointConnection Receiver{ nullptr };

        std::mutex Lock{};
        std::vector<std::array<uint32_t, 4>> Messages{};
        std::vector<uint8_t> WordCounts{};

        winrt::event_token Token{};

        void Start()
        {
            Session = MidiSession::Create(L"Sequence builder tests");
            VERIFY_IS_NOT_NULL(Session);

            Sender = Session.CreateEndpointConnection(SequencingTests::LoopbackAEndpointId());
            VERIFY_IS_NOT_NULL(Sender);
            VERIFY_IS_TRUE(Sender.Open());

            Receiver = Session.CreateEndpointConnection(SequencingTests::LoopbackBEndpointId());
            VERIFY_IS_NOT_NULL(Receiver);

            Token = Receiver.MessageReceived([this](auto&&, auto&& args)
                {
                    std::array<uint32_t, 4> words{};

                    auto const count = args.FillWords(words[0], words[1], words[2], words[3]);

                    std::lock_guard<std::mutex> const guard{ Lock };

                    Messages.push_back(words);
                    WordCounts.push_back(count);
                });

            VERIFY_IS_TRUE(Receiver.Open());
        }

        void Stop()
        {
            if (Receiver != nullptr) { Receiver.MessageReceived(Token); }
            if (Session != nullptr) { Session.Close(); Session = nullptr; }

            Sender = nullptr;
            Receiver = nullptr;
        }

        ~BuilderCapture() { Stop(); }

        std::vector<std::array<uint32_t, 4>> Snapshot()
        {
            std::lock_guard<std::mutex> const guard{ Lock };
            return Messages;
        }

        std::vector<uint8_t> CountSnapshot()
        {
            std::lock_guard<std::mutex> const guard{ Lock };
            return WordCounts;
        }
    };

    bool WaitFor(_In_ std::function<bool()> const& satisfied, _In_ uint32_t const timeoutMilliseconds)
    {
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (satisfied()) { return true; }

            ::Sleep(PollIntervalMilliseconds);
        }

        return satisfied();
    }

    MidiChannel ChannelOf(uint8_t index)
    {
        return MidiChannel{ static_cast<uint8_t>(index) };
    }

    bool IsNoteOn(uint32_t word)
    {
        return ((word >> 28) & 0xF) == 0x2 && ((word >> 20) & 0xF) == 0x9 && (word & 0x7F) > 0;
    }

    bool IsNoteOff(uint32_t word)
    {
        auto const type = (word >> 28) & 0xF;
        auto const status = (word >> 20) & 0xF;

        return type == 0x2 && (status == 0x8 || (status == 0x9 && (word & 0x7F) == 0));
    }
}

void MidiSequenceBuilderTests::BuildsAnEmptySequence()
{
    MidiSequenceBuilder builder{};

    auto const sequence = builder.GetSequence();

    // Empty, but real: a caller gets an object it can ask questions of rather than null.
    VERIFY_IS_NOT_NULL(sequence);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.EventCount());
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.NoteCount());
}

void MidiSequenceBuilderTests::AddsTracksAndReportsTheirIndexes()
{
    MidiSequenceBuilder builder{};

    VERIFY_ARE_EQUAL(uint16_t{ 0 }, builder.AddTrack(L"Bass"));
    VERIFY_ARE_EQUAL(uint16_t{ 1 }, builder.AddTrack(L"Lead"));

    auto const sequence = builder.GetSequence();

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sequence.Tracks().Size());
    VERIFY_ARE_EQUAL(winrt::hstring{ L"Bass" }, sequence.Tracks().GetAt(0).Name());
    VERIFY_ARE_EQUAL(winrt::hstring{ L"Lead" }, sequence.Tracks().GetAt(1).Name());
}

void MidiSequenceBuilderTests::AddNoteWritesBothHalves()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Track");

    builder.AddNote(0, 480, track, ChannelOf(0), 60, 100);

    auto const sequence = builder.GetSequence();

    // Two events, and a paired note: the pairing is what proves the note cannot hang.
    VERIFY_ARE_EQUAL(uint32_t{ 2 }, sequence.EventCount());
    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sequence.NoteCount());

    std::vector<MidiSequenceNote> notes(4);

    auto const filled = sequence.FillNotesInTickRange(0, 1000, 0, notes);

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, filled);
    VERIFY_ARE_EQUAL(uint32_t{ 0 }, notes[0].StartTick);
    VERIFY_ARE_EQUAL(uint32_t{ 480 }, notes[0].EndTick);
    VERIFY_ARE_EQUAL(uint8_t{ 60 }, notes[0].NoteNumber);
    VERIFY_ARE_EQUAL(uint8_t{ 100 }, notes[0].Velocity);
}

void MidiSequenceBuilderTests::AddNoteWithNoDurationStillEnds()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Track");

    builder.AddNote(100, 0, track, ChannelOf(3), 64, 90);

    auto const sequence = builder.GetSequence();

    std::vector<MidiSequenceNote> notes(4);

    VERIFY_ARE_EQUAL(uint32_t{ 1 }, sequence.FillNotesInTickRange(0, 1000, 0, notes));

    // A zero length note would pair with itself and never end.
    VERIFY_IS_GREATER_THAN(notes[0].EndTick, notes[0].StartTick);
}

void MidiSequenceBuilderTests::CarriesTempoAndTimeSignature()
{
    MidiSequenceBuilder builder{};

    builder.TicksPerQuarterNote(960);

    auto const track = builder.AddTrack(L"Track");

    builder.AddTempoChange(0, 140.0);
    builder.AddTimeSignature(0, 3, 4);
    builder.AddNote(0, 960, track, ChannelOf(0), 60, 100);

    auto const sequence = builder.GetSequence();

    VERIFY_ARE_EQUAL(uint16_t{ 960 }, sequence.TicksPerQuarterNote());
    VERIFY_IS_LESS_THAN(std::abs(sequence.GetBeatsPerMinuteAtTick(0) - 140.0), 0.5);

    // One quarter note at 140 beats per minute is about 428571 microseconds.
    auto const atOneBeat = sequence.ConvertTickToMicroseconds(960);

    VERIFY_IS_GREATER_THAN(atOneBeat, uint64_t{ 400000 });
    VERIFY_IS_LESS_THAN(atOneBeat, uint64_t{ 460000 });
}

void MidiSequenceBuilderTests::RejectsMalformedInput()
{
    MidiSequenceBuilder builder{};

    // Everything here is something a caller can legitimately get wrong. None of it may throw,
    // and none of it may end up in the sequence.
    builder.AddNote(0, 100, 0, ChannelOf(0), 60, 100);            // no track exists yet

    auto const track = builder.AddTrack(L"Track");

    builder.AddNote(0, 100, 99, ChannelOf(0), 60, 100);           // track index out of range
    builder.AddNote(0, 100, track, nullptr, 60, 100);             // null channel
    builder.AddNote(0, 100, track, ChannelOf(0), 200, 100);       // note number out of range

    builder.AddSystemExclusive(track, 0, std::vector<uint8_t>{ 0x7E, 0x00 });          // no F0
    builder.AddSystemExclusive(track, 0, std::vector<uint8_t>{ 0xF0, 0x7E });          // no F7
    builder.AddSystemExclusive(track, 0, std::vector<uint8_t>{});                      // empty

    builder.AddMessages(track, 0, std::vector<uint32_t>{});                            // no words
    builder.AddMessages(99, 0, std::vector<uint32_t>{ 0x20903C64 });                   // bad track

    builder.AddTempoChange(0, 0.0);                               // zero tempo
    builder.AddTempoChange(0, -50.0);                             // negative tempo
    builder.AddTimeSignature(0, 0, 4);                            // zero numerator
    builder.AddTimeSignature(0, 4, 5);                            // denominator not a power of two

    builder.TicksPerQuarterNote(0);                               // would divide by zero later

    auto const sequence = builder.GetSequence();

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, sequence.EventCount());
    VERIFY_IS_GREATER_THAN(sequence.TicksPerQuarterNote(), uint16_t{ 0 });
}

void MidiSequenceBuilderTests::ClearEmptiesTheBuilder()
{
    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Track");
    builder.AddNote(0, 100, track, ChannelOf(0), 60, 100);

    VERIFY_ARE_EQUAL(uint32_t{ 2 }, builder.GetSequence().EventCount());

    builder.Clear();

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, builder.GetSequence().EventCount());
}

void MidiSequenceBuilderTests::PlaysABuiltSequence()
{
    BuilderCapture capture{};
    capture.Start();

    MidiSequenceBuilder builder{};

    builder.TicksPerQuarterNote(480);
    builder.AddTempoChange(0, 240.0);

    auto const track = builder.AddTrack(L"Track");

    builder.AddNote(0, 120, track, ChannelOf(0), 60, 100);
    builder.AddNote(240, 120, track, ChannelOf(0), 64, 100);

    MidiSequencePlayer player{ capture.Sender, SequencingTests::FirstGroup() };

    player.SetSequenceAsync(builder.GetSequence()).get();
    player.Play();

    auto const sawBoth = WaitFor([&capture]()
        {
            int noteOns = 0;

            for (auto const& message : capture.Snapshot())
            {
                if (IsNoteOn(message[0])) { noteOns++; }
            }

            return noteOns >= 2;
        },
        6000);

    VERIFY_IS_TRUE(sawBoth);

    // Nothing may be left sounding.
    auto const endedCleanly = WaitFor([&capture]()
        {
            int noteOns = 0;
            int noteOffs = 0;

            for (auto const& message : capture.Snapshot())
            {
                if (IsNoteOn(message[0])) { noteOns++; }
                else if (IsNoteOff(message[0])) { noteOffs++; }
            }

            return noteOffs >= noteOns;
        },
        6000);

    VERIFY_IS_TRUE(endedCleanly);

    player.Close();
}

void MidiSequenceBuilderTests::CarriesUniversalPacketsThroughUnconverted()
{
    // The point of the whole change: a message MIDI 1.0 cannot express, stored and played without
    // being converted. This is a MIDI 2.0 channel voice note on with 16 bit velocity.
    BuilderCapture capture{};
    capture.Start();

    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Track");

    std::vector<uint32_t> const noteOn{ 0x40903C00u, 0xFFFF0000u };

    builder.AddMessages(track, 0, noteOn);

    MidiSequencePlayer player{ capture.Sender, SequencingTests::FirstGroup() };

    player.SetSequenceAsync(builder.GetSequence()).get();
    player.Play();

    auto const arrived = WaitFor([&capture]()
        {
            auto const messages = capture.Snapshot();
            auto const counts = capture.CountSnapshot();

            for (size_t i = 0; i < messages.size() && i < counts.size(); i++)
            {
                if (counts[i] == 2 && messages[i][0] == 0x40903C00u && messages[i][1] == 0xFFFF0000u)
                {
                    return true;
                }
            }

            return false;
        },
        6000);

    // Both words, byte for byte. A converted message would have arrived as one 32 bit word with
    // the velocity squashed to 7 bits.
    VERIFY_IS_TRUE(arrived);

    player.Close();
}

void MidiSequenceBuilderTests::StampsTheGroupOnUniversalPackets()
{
    BuilderCapture capture{};
    capture.Start();

    MidiSequenceBuilder builder{};

    auto const track = builder.AddTrack(L"Track");

    // Written for group 0; the player is on group 6, and the group belongs to where it is played.
    std::vector<uint32_t> const noteOn{ 0x40903C00u, 0xFFFF0000u };

    builder.AddMessages(track, 0, noteOn);

    MidiSequencePlayer player{ capture.Sender, MidiGroup{ static_cast<uint8_t>(6) } };

    player.SetSequenceAsync(builder.GetSequence()).get();
    player.Play();

    auto const arrived = WaitFor([&capture]()
        {
            for (auto const& message : capture.Snapshot())
            {
                // Everything but the group has to be untouched, and the group has to be the
                // player's rather than the one the words were written with.
                if ((message[0] & 0xF0FFFFFFu) == (0x40903C00u & 0xF0FFFFFFu) &&
                    ((message[0] >> 24) & 0x0F) == 6)
                {
                    return true;
                }
            }

            return false;
        },
        6000);

    VERIFY_IS_TRUE(arrived);

    player.Close();
}

void MidiSequenceBuilderTests::AbsoluteTimingWaitsInRealTime()
{
    BuilderCapture capture{};
    capture.Start();

    MidiSequenceBuilder builder{};

    builder.TimingMode(MidiSequenceTimingMode::Absolute);

    auto const track = builder.AddTrack(L"Track");

    // A tick is a microsecond here, so these are 0 ms and 900 ms, and no tempo applies.
    builder.AddNote(0, 1000, track, ChannelOf(0), 60, 100);
    builder.AddNote(900000, 1000, track, ChannelOf(0), 67, 100);

    auto const sequence = builder.GetSequence();

    // The model has to agree before anything is played: a tick is a microsecond.
    VERIFY_ARE_EQUAL(uint64_t{ 900000 }, sequence.ConvertTickToMicroseconds(900000));

    MidiSequencePlayer player{ capture.Sender, SequencingTests::FirstGroup() };

    player.SetSequenceAsync(sequence).get();

    auto const start = std::chrono::steady_clock::now();

    player.Play();

    VERIFY_IS_TRUE(WaitFor([&capture]()
        {
            int noteOns = 0;

            for (auto const& message : capture.Snapshot())
            {
                if (IsNoteOn(message[0])) { noteOns++; }
            }

            return noteOns >= 2;
        },
        6000));

    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // Generous at both ends: the point is that the gap was a real gap and not a musical one. At
    // the default tempo those ticks would have been hours apart, and with no timing mode at all
    // they would have arrived together.
    VERIFY_IS_GREATER_THAN(elapsed, 600LL);
    VERIFY_IS_LESS_THAN(elapsed, 4000LL);

    player.Close();
}
