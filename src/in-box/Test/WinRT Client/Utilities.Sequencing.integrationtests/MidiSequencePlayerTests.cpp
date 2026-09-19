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
    // Tests here must not sit waiting for music. Every wait is bounded and every fixture is
    // short, so a failure costs seconds rather than minutes.
    constexpr uint32_t PollIntervalMilliseconds = 20;

    struct CaptureSession
    {
        MidiSession Session{ nullptr };
        MidiEndpointConnection Sender{ nullptr };
        MidiEndpointConnection Receiver{ nullptr };

        std::mutex Lock{};
        std::vector<std::array<uint32_t, 4>> Messages{};
        std::vector<uint8_t> WordCounts{};

        winrt::event_token Token{};

        void Start(_In_ bool withReceiver = true)
        {
            Session = MidiSession::Create(L"Sequencing tests");

            VERIFY_IS_NOT_NULL(Session);

            Sender = Session.CreateEndpointConnection(LoopbackAEndpointId());
            VERIFY_IS_NOT_NULL(Sender);
            VERIFY_IS_TRUE(Sender.Open());

            if (!withReceiver)
            {
                return;
            }

            Receiver = Session.CreateEndpointConnection(LoopbackBEndpointId());
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
            if (Receiver != nullptr)
            {
                Receiver.MessageReceived(Token);
            }

            if (Session != nullptr)
            {
                Session.Close();
                Session = nullptr;
            }

            Sender = nullptr;
            Receiver = nullptr;
        }

        ~CaptureSession() { Stop(); }

        size_t Count()
        {
            std::lock_guard<std::mutex> const guard{ Lock };
            return Messages.size();
        }

        std::vector<std::array<uint32_t, 4>> Snapshot()
        {
            std::lock_guard<std::mutex> const guard{ Lock };
            return Messages;
        }

        void Clear()
        {
            std::lock_guard<std::mutex> const guard{ Lock };
            Messages.clear();
            WordCounts.clear();
        }
    };

    // MIDI 1.0 channel voice messages arrive as one 32 bit word: type 2, then group, then status
    // and channel, then the two data bytes.
    bool IsNoteOnWithVelocity(_In_ uint32_t const word)
    {
        auto const messageType = (word >> 28) & 0xF;
        auto const status = (word >> 20) & 0xF;
        auto const velocity = word & 0x7F;

        return messageType == 0x2 && status == 0x9 && velocity > 0;
    }

    bool IsControlChange(_In_ uint32_t const word, _In_ uint8_t const controller)
    {
        auto const messageType = (word >> 28) & 0xF;
        auto const status = (word >> 20) & 0xF;
        auto const index = (word >> 8) & 0x7F;

        return messageType == 0x2 && status == 0xB && index == controller;
    }

    uint8_t ChannelOf(_In_ uint32_t const word)
    {
        return static_cast<uint8_t>((word >> 16) & 0xF);
    }

    bool WaitForState(
        _In_ MidiSequencePlayer const& player,
        _In_ MidiSequencePlayerState const state,
        _In_ uint32_t const timeoutMilliseconds)
    {
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (player.State() == state)
            {
                return true;
            }

            ::Sleep(PollIntervalMilliseconds);
        }

        return player.State() == state;
    }

    MidiSequencePlayer MakeBorrowingPlayer(_In_ CaptureSession& capture)
    {
        return MidiSequencePlayer{ capture.Sender, FirstGroup() };
    }

    // Waiting on the messages themselves rather than on a fixed sleep. Delivery latency through
    // the service is not something a test should be guessing at, and polling returns as soon as
    // what was wanted has arrived.
    bool WaitForMessages(
        _In_ CaptureSession& capture,
        _In_ size_t const startIndex,
        _In_ std::function<bool(std::vector<std::array<uint32_t, 4>> const&, size_t)> const& satisfied,
        _In_ uint32_t const timeoutMilliseconds)
    {
        auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);

        while (std::chrono::steady_clock::now() < deadline)
        {
            auto const messages = capture.Snapshot();

            if (satisfied(messages, startIndex))
            {
                return true;
            }

            ::Sleep(PollIntervalMilliseconds);
        }

        return satisfied(capture.Snapshot(), startIndex);
    }
}

void MidiSequencePlayerTests::BorrowsAConnectionAndLeavesItOpen()
{
    CaptureSession capture{};
    capture.Start(false);

    {
        auto player = MakeBorrowingPlayer(capture);

        VERIFY_IS_FALSE(player.OwnsConnection());
        VERIFY_IS_NOT_NULL(player.Connection());

        player.Close();
    }

    // Closing the player must not have taken the caller's connection with it.
    VERIFY_IS_NOT_NULL(capture.Sender);
    VERIFY_ARE_EQUAL(
        MidiSendMessageResults::Succeeded,
        capture.Sender.SendSingleMessageWords(0, 0x20903C64) & MidiSendMessageResults::Succeeded);
}

void MidiSequencePlayerTests::OpensAndOwnsAConnectionForAnEndpoint()
{
    auto session = MidiSession::Create(L"Sequencing tests owning");

    VERIFY_IS_NOT_NULL(session);

    auto player = MidiSequencePlayer::CreateForEndpointAsync(session, LoopbackAEndpointId(), FirstGroup()).get();

    VERIFY_IS_NOT_NULL(player);
    VERIFY_IS_TRUE(player.OwnsConnection());
    VERIFY_IS_NOT_NULL(player.Connection());

    player.Close();

    session.Close();
}

void MidiSequencePlayerTests::RefusesAnEndpointThatDoesNotExist()
{
    auto session = MidiSession::Create(L"Sequencing tests missing endpoint");

    VERIFY_IS_NOT_NULL(session);

    auto player = MidiSequencePlayer::CreateForEndpointAsync(
        session,
        winrt::hstring{ L"\\\\?\\SWD#MIDISRV#MIDIU_NOTHING_HERE#{e7cce071-3c03-423f-88d3-f1045d02552b}" },
        FirstGroup()).get();

    VERIFY_IS_NULL(player);

    session.Close();
}

void MidiSequencePlayerTests::ClosingTwiceIsHarmless()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.Close();
    player.Close();
}

void MidiSequencePlayerTests::PlaysAShortFileToTheEnd()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    player.SetSequenceAsync(sequence).get();

    VERIFY_IS_NOT_NULL(player.Sequence());
    VERIFY_IS_TRUE(player.State() == MidiSequencePlayerState::Stopped);

    player.Play();

    // Two seconds of music. Allow a generous margin and still fail fast.
    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 6000));

    VERIFY_IS_TRUE(capture.Count() > 0);

    player.Close();
}

void MidiSequencePlayerTests::ReportsPositionWhilePlaying()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    auto const idle = player.Position();

    VERIFY_ARE_EQUAL(uint64_t{ 0 }, idle.Microseconds);
    VERIFY_ARE_EQUAL(uint64_t{ 2000000 }, idle.DurationMicroseconds);
    VERIFY_IS_TRUE(idle.State == MidiSequencePlayerState::Stopped);

    player.Play();

    ::Sleep(500);

    auto const moving = player.Position();

    VERIFY_IS_TRUE(moving.Microseconds > 0);
    VERIFY_IS_TRUE(moving.Microseconds < moving.DurationMicroseconds);
    VERIFY_IS_TRUE(moving.State == MidiSequencePlayerState::Playing);
    VERIFY_ARE_EQUAL(120.0, moving.BeatsPerMinute);
    VERIFY_IS_TRUE(moving.Bar >= 1);
    VERIFY_IS_TRUE(moving.Beat >= 1);

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::PausesAndResumes()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    player.Play();
    ::Sleep(300);

    player.Pause();

    VERIFY_IS_TRUE(player.State() == MidiSequencePlayerState::Paused);

    auto const paused = player.Position().Microseconds;

    ::Sleep(300);

    // A paused player does not advance.
    VERIFY_ARE_EQUAL(paused, player.Position().Microseconds);

    player.Play();
    ::Sleep(200);

    VERIFY_IS_TRUE(player.Position().Microseconds > paused);

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::StopsAndRewinds()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    player.Play();
    ::Sleep(300);

    player.Stop();

    VERIFY_IS_TRUE(player.State() == MidiSequencePlayerState::Stopped);
    VERIFY_ARE_EQUAL(uint64_t{ 0 }, player.Position().Microseconds);

    player.Close();
}

void MidiSequencePlayerTests::SeeksByTickAndByTime()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    player.SetSequenceAsync(sequence).get();

    player.SeekToMicroseconds(1000000);
    VERIFY_ARE_EQUAL(uint64_t{ 1000000 }, player.Position().Microseconds);

    player.SeekToTick(480);
    VERIFY_ARE_EQUAL(sequence.ConvertTickToMicroseconds(480), player.Position().Microseconds);

    player.Close();
}

void MidiSequencePlayerTests::RaisesPlaybackEnded()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    wil::unique_event ended{};
    ended.create();

    auto const token = player.PlaybackEnded([&ended](auto&&, auto&&) { ended.SetEvent(); });

    player.SetSequenceAsync(ReadTestSequence(L"smpte-timing.mid")).get();

    player.Play();

    // A tenth of a second of music. Three seconds is a wide margin.
    VERIFY_IS_TRUE(ended.wait(3000));

    player.PlaybackEnded(token);
    player.Close();
}

void MidiSequencePlayerTests::SendsNotesToTheEndpoint()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    player.Play();

    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 6000));

    // Wait for all eight to arrive rather than assuming a delivery time.
    WaitForMessages(capture, 0,
        [](auto const& messages, size_t)
        {
            size_t noteOns{ 0 };

            for (auto const& message : messages)
            {
                if (IsNoteOnWithVelocity(message[0])) { ++noteOns; }
            }

            return noteOns >= 8;
        },
        3000);

    auto const messages = capture.Snapshot();

    size_t noteOns{ 0 };

    for (auto const& message : messages)
    {
        if (IsNoteOnWithVelocity(message[0]))
        {
            ++noteOns;
        }
    }

    LOG_OUTPUT(L"captured %d messages, %d note ons", static_cast<int>(messages.size()), static_cast<int>(noteOns));

    VERIFY_ARE_EQUAL(size_t{ 8 }, noteOns);

    player.Close();
}

void MidiSequencePlayerTests::SilencesEverythingOnStop()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    // Long enough that it is certainly still playing when stopped. A file shorter than the stop
    // delay would end on its own and the stop would have nothing to silence.
    auto const sequence = ReadTestSequence(L"multi-track.mid");

    LOG_OUTPUT(L"channel mask 0x%04X, tracks %d, notes %d",
        static_cast<int>(sequence.UsedChannelMask()),
        static_cast<int>(sequence.Tracks().Size()),
        static_cast<int>(sequence.NoteCount()));

    player.SetSequenceAsync(sequence).get();

    player.Play();
    ::Sleep(250);

    VERIFY_IS_TRUE(player.State() == MidiSequencePlayerState::Playing);

    auto const beforeStop = capture.Count();

    player.Stop();

    // Every channel the sequence uses gets sustain off, all notes off, all sound off and pitch
    // bend center, so waiting for the last of those is waiting for the whole panic.
    auto const sawPanic = WaitForMessages(capture, beforeStop,
        [](auto const& messages, size_t from)
        {
            for (size_t index = from; index < messages.size(); ++index)
            {
                if (IsControlChange(messages[index][0], 120))
                {
                    return true;
                }
            }

            return false;
        },
        4000);

    auto const messages = capture.Snapshot();

    LOG_OUTPUT(L"%d messages before stop, %d after",
        static_cast<int>(beforeStop), static_cast<int>(messages.size() - beforeStop));

    VERIFY_IS_TRUE(sawPanic);

    bool sawAllNotesOff{ false };
    bool sawAllSoundOff{ false };
    bool sawSustainOff{ false };
    size_t lastPanicMessage{ 0 };

    for (size_t index = beforeStop; index < messages.size(); ++index)
    {
        auto const word = messages[index][0];

        if (IsControlChange(word, 123)) { sawAllNotesOff = true; lastPanicMessage = index; }
        if (IsControlChange(word, 120)) { sawAllSoundOff = true; lastPanicMessage = index; }
        if (IsControlChange(word, 64) && (word & 0x7F) == 0) { sawSustainOff = true; lastPanicMessage = index; }
    }

    // Sustain has to be released before All Notes Off, or a device holding the pedal keeps
    // sounding straight through it.
    VERIFY_IS_TRUE(sawSustainOff);
    VERIFY_IS_TRUE(sawAllNotesOff);
    VERIFY_IS_TRUE(sawAllSoundOff);

    // Notes already handed to the service inside the look ahead window cannot be recalled, so a
    // few may still sound after the stop. What matters is that the second panic pass lands after
    // them and nothing is left sounding once it has.
    size_t noteOnsAfterLastPanic{ 0 };

    for (size_t index = lastPanicMessage; index < messages.size(); ++index)
    {
        if (IsNoteOnWithVelocity(messages[index][0]))
        {
            ++noteOnsAfterLastPanic;
        }
    }

    VERIFY_ARE_EQUAL(size_t{ 0 }, noteOnsAfterLastPanic);

    player.Close();
}

void MidiSequencePlayerTests::SilencesHangingNotesAtTheEndOfAFile()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    // This file deliberately ends with the sustain pedal down and four notes never released.
    player.SetSequenceAsync(ReadTestSequence(L"hanging-notes.mid")).get();

    player.Play();

    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 5000));

    VERIFY_IS_TRUE(WaitForMessages(capture, 0,
        [](auto const& messages, size_t)
        {
            for (auto const& message : messages)
            {
                if (IsControlChange(message[0], 123)) { return true; }
            }

            return false;
        },
        4000));

    auto const messages = capture.Snapshot();

    VERIFY_IS_TRUE(messages.size() > 0);

    // Find where the silencing began, then prove nothing sounds after it.
    size_t firstAllNotesOff{ messages.size() };

    for (size_t index = 0; index < messages.size(); ++index)
    {
        if (IsControlChange(messages[index][0], 123))
        {
            firstAllNotesOff = index;
            break;
        }
    }

    VERIFY_IS_TRUE(firstAllNotesOff < messages.size());

    for (size_t index = firstAllNotesOff; index < messages.size(); ++index)
    {
        VERIFY_IS_FALSE(IsNoteOnWithVelocity(messages[index][0]));
    }

    player.Close();
}

void MidiSequencePlayerTests::MutingATrackStopsItsNotes()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    auto const sequence = ReadTestSequence(L"multi-track.mid");

    player.SetSequenceAsync(sequence).get();

    // Track 1 is the first sounding part and uses channel 1.
    player.SetTrackMuted(1, true);

    VERIFY_IS_TRUE(player.IsTrackMuted(1));
    VERIFY_IS_FALSE(player.IsTrackMuted(2));

    player.Play();

    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 6000));

    WaitForMessages(capture, 0,
        [](auto const& messages, size_t)
        {
            for (auto const& message : messages)
            {
                if (IsControlChange(message[0], 120)) { return true; }
            }

            return false;
        },
        4000);

    auto const messages = capture.Snapshot();

    bool sawChannelZeroNoteOn{ false };
    bool sawChannelOneNoteOn{ false };

    for (auto const& message : messages)
    {
        if (!IsNoteOnWithVelocity(message[0]))
        {
            continue;
        }

        if (ChannelOf(message[0]) == 0) { sawChannelZeroNoteOn = true; }
        if (ChannelOf(message[0]) == 1) { sawChannelOneNoteOn = true; }
    }

    // The muted track is silent; the others still play.
    VERIFY_IS_FALSE(sawChannelZeroNoteOn);
    VERIFY_IS_TRUE(sawChannelOneNoteOn);

    player.Close();
}

void MidiSequencePlayerTests::SoloingATrackSilencesTheOthers()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    player.SoloTrackIndex(2);

    VERIFY_ARE_EQUAL(2, player.SoloTrackIndex());

    player.Play();

    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 6000));

    WaitForMessages(capture, 0,
        [](auto const& messages, size_t)
        {
            for (auto const& message : messages)
            {
                if (IsControlChange(message[0], 120)) { return true; }
            }

            return false;
        },
        4000);

    auto const messages = capture.Snapshot();

    std::array<bool, 16> channelPlayed{};

    for (auto const& message : messages)
    {
        if (IsNoteOnWithVelocity(message[0]))
        {
            channelPlayed[ChannelOf(message[0])] = true;
        }
    }

    // Track 2 writes to channel 1. Nothing else should sound.
    VERIFY_IS_TRUE(channelPlayed[1]);
    VERIFY_IS_FALSE(channelPlayed[0]);
    VERIFY_IS_FALSE(channelPlayed[2]);

    player.SoloTrackIndex(-1);
    VERIFY_ARE_EQUAL(-1, player.SoloTrackIndex());

    player.Close();
}

void MidiSequencePlayerTests::RoundTripsTrackRouting()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};

    routing.Group(MidiGroup{ static_cast<uint8_t>(3) });
    routing.IsMuted(true);

    player.SetTrackRouting(1, routing);

    auto const read = player.GetTrackRouting(1);

    VERIFY_IS_NOT_NULL(read);
    VERIFY_IS_NOT_NULL(read.Group());
    VERIFY_ARE_EQUAL(uint8_t{ 3 }, read.Group().Index());
    VERIFY_IS_TRUE(read.IsMuted());

    player.Close();
}

void MidiSequencePlayerTests::RoutingMuteAgreesWithTrackMute()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};
    routing.IsMuted(true);

    player.SetTrackRouting(2, routing);

    // Setting routing with a mute has to be the same statement as muting the track.
    VERIFY_IS_TRUE(player.IsTrackMuted(2));

    player.Close();
}

void MidiSequencePlayerTests::TransportCallsWithoutASequenceAreHarmless()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    VERIFY_IS_TRUE(player.State() == MidiSequencePlayerState::NoSequence);

    // None of these have anything to act on. None may throw.
    player.Play();
    player.Pause();
    player.Stop();
    player.SeekToMicroseconds(5000);
    player.SeekToTick(100);
    player.SilenceAllNotes();

    VERIFY_IS_NULL(player.Sequence());

    player.Close();
}

void MidiSequencePlayerTests::SettingANullSequenceClearsIt()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();
    VERIFY_IS_NOT_NULL(player.Sequence());

    player.SetSequenceAsync(nullptr).get();
    VERIFY_IS_NULL(player.Sequence());

    player.Close();
}

void MidiSequencePlayerTests::OutOfRangeTrackIndexesAreHarmless()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    // One track in this file. Everything beyond it has to be ignored rather than fault.
    player.SetTrackMuted(9000, true);
    VERIFY_IS_FALSE(player.IsTrackMuted(9000));

    player.SoloTrackIndex(9000);
    VERIFY_ARE_EQUAL(-1, player.SoloTrackIndex());

    VERIFY_IS_NOT_NULL(player.GetTrackRouting(9000));

    player.SetTrackRouting(9000, nullptr);

    player.Close();
}

void MidiSequencePlayerTests::SeekingBeyondTheEndIsClamped()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    auto const sequence = ReadTestSequence(L"plain-scale.mid");

    player.SetSequenceAsync(sequence).get();

    player.SeekToMicroseconds(9999999999ull);

    VERIFY_IS_TRUE(player.Position().Microseconds <= sequence.DurationMicroseconds());

    player.SeekToTick(0xFFFFFFFF);

    VERIFY_IS_TRUE(player.Position().Microseconds <= sequence.DurationMicroseconds());

    player.Close();
}
