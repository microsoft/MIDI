// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

#include <algorithm>
#include <array>
#include <limits>

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

    uint8_t GroupOf(_In_ uint32_t const word)
    {
        return static_cast<uint8_t>((word >> 24) & 0xF);
    }

    // The far end of the same loopback pair. Sending into B comes out of A, which is what lets
    // one pair of built-in endpoints stand in for two separate instruments.
    struct ReverseCaptureSession
    {
        MidiSession Session{ nullptr };
        MidiEndpointConnection Sender{ nullptr };
        MidiEndpointConnection Receiver{ nullptr };

        std::mutex Lock{};
        std::vector<std::array<uint32_t, 4>> Messages{};

        winrt::event_token Token{};

        void Start()
        {
            Session = MidiSession::Create(L"Sequencing tests, second endpoint");
            VERIFY_IS_NOT_NULL(Session);

            Sender = Session.CreateEndpointConnection(LoopbackBEndpointId());
            VERIFY_IS_NOT_NULL(Sender);
            VERIFY_IS_TRUE(Sender.Open());

            Receiver = Session.CreateEndpointConnection(LoopbackAEndpointId());
            VERIFY_IS_NOT_NULL(Receiver);

            Token = Receiver.MessageReceived([this](auto&&, auto&& args)
                {
                    std::array<uint32_t, 4> words{};

                    args.FillWords(words[0], words[1], words[2], words[3]);

                    std::lock_guard<std::mutex> const guard{ Lock };

                    Messages.push_back(words);
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

        ~ReverseCaptureSession() { Stop(); }

        std::vector<std::array<uint32_t, 4>> Snapshot()
        {
            std::lock_guard<std::mutex> const guard{ Lock };
            return Messages;
        }
    };

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

namespace
{
    // multi-track.mid writes track 1 to channel 0 and track 2 to channel 1, which is what the
    // mute and solo tests above rely on as well.
    constexpr uint16_t ChannelZeroTrack = 1;
    constexpr uint16_t ChannelOneTrack = 2;

    // Every routing test plays the whole file and then waits for the all sound off that ends it,
    // so the assertions run against a complete capture rather than a partial one.
    void PlayToTheEnd(_In_ MidiSequencePlayer const& player, _In_ CaptureSession& capture)
    {
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
    }
}

void MidiSequencePlayerTests::RoutesATrackToADifferentGroup()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};
    routing.Group(MidiGroup{ static_cast<uint8_t>(5) });

    player.SetTrackRouting(ChannelZeroTrack, routing);

    PlayToTheEnd(player, capture);

    bool routedTrackOnGroupFive{ false };
    bool routedTrackOnGroupZero{ false };
    bool otherTrackOnGroupZero{ false };

    for (auto const& message : capture.Snapshot())
    {
        if (!IsNoteOnWithVelocity(message[0]))
        {
            continue;
        }

        if (ChannelOf(message[0]) == 0)
        {
            if (GroupOf(message[0]) == 5) { routedTrackOnGroupFive = true; }
            if (GroupOf(message[0]) == 0) { routedTrackOnGroupZero = true; }
        }
        else if (ChannelOf(message[0]) == 1 && GroupOf(message[0]) == 0)
        {
            otherTrackOnGroupZero = true;
        }
    }

    // The routed track moved, and only the routed track moved.
    VERIFY_IS_TRUE(routedTrackOnGroupFive);
    VERIFY_IS_FALSE(routedTrackOnGroupZero);
    VERIFY_IS_TRUE(otherTrackOnGroupZero);

    player.Close();
}

void MidiSequencePlayerTests::RoutesATrackToADifferentChannel()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};
    routing.ChannelOverride(MidiChannel{ static_cast<uint8_t>(9) });

    player.SetTrackRouting(ChannelZeroTrack, routing);

    PlayToTheEnd(player, capture);

    std::array<bool, 16> noteOnChannels{};

    for (auto const& message : capture.Snapshot())
    {
        if (IsNoteOnWithVelocity(message[0]))
        {
            noteOnChannels[ChannelOf(message[0])] = true;
        }
    }

    // The track the file wrote to channel 0 now plays on channel 9, and nothing is left behind
    // on channel 0. The untouched track still plays where it always did.
    VERIFY_IS_TRUE(noteOnChannels[9]);
    VERIFY_IS_FALSE(noteOnChannels[0]);
    VERIFY_IS_TRUE(noteOnChannels[1]);

    player.Close();
}

void MidiSequencePlayerTests::RoutesTwoTracksToTwoEndpointsAtOnce()
{
    // This is what MidiSequenceTrackRouting has always documented and never done: one file
    // driving two instruments at the same time.
    CaptureSession capture{};
    capture.Start();

    ReverseCaptureSession second{};
    second.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};
    routing.Connection(second.Sender);

    player.SetTrackRouting(ChannelOneTrack, routing);

    PlayToTheEnd(player, capture);

    bool firstEndpointSawChannelZero{ false };
    bool firstEndpointSawChannelOne{ false };

    for (auto const& message : capture.Snapshot())
    {
        if (!IsNoteOnWithVelocity(message[0])) { continue; }

        if (ChannelOf(message[0]) == 0) { firstEndpointSawChannelZero = true; }
        if (ChannelOf(message[0]) == 1) { firstEndpointSawChannelOne = true; }
    }

    bool secondEndpointSawChannelOne{ false };
    bool secondEndpointSawChannelZero{ false };

    for (auto const& message : second.Snapshot())
    {
        if (!IsNoteOnWithVelocity(message[0])) { continue; }

        if (ChannelOf(message[0]) == 1) { secondEndpointSawChannelOne = true; }
        if (ChannelOf(message[0]) == 0) { secondEndpointSawChannelZero = true; }
    }

    // Each endpoint got its own track and neither got the other's.
    VERIFY_IS_TRUE(firstEndpointSawChannelZero);
    VERIFY_IS_FALSE(firstEndpointSawChannelOne);
    VERIFY_IS_TRUE(secondEndpointSawChannelOne);
    VERIFY_IS_FALSE(secondEndpointSawChannelZero);

    player.Close();
}

void MidiSequencePlayerTests::ClearingRoutingSendsTheTrackBackToThePlayerConnection()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};
    routing.Group(MidiGroup{ static_cast<uint8_t>(7) });

    player.SetTrackRouting(ChannelZeroTrack, routing);
    player.SetTrackRouting(ChannelZeroTrack, nullptr);

    PlayToTheEnd(player, capture);

    bool onGroupZero{ false };
    bool onGroupSeven{ false };

    for (auto const& message : capture.Snapshot())
    {
        if (!IsNoteOnWithVelocity(message[0]) || ChannelOf(message[0]) != 0)
        {
            continue;
        }

        if (GroupOf(message[0]) == 0) { onGroupZero = true; }
        if (GroupOf(message[0]) == 7) { onGroupSeven = true; }
    }

    VERIFY_IS_TRUE(onGroupZero);
    VERIFY_IS_FALSE(onGroupSeven);

    player.Close();
}

void MidiSequencePlayerTests::SilencesEveryEndpointATrackWasRoutedTo()
{
    // A note left sounding on an endpoint the panic skipped is the whole reason a panic exists.
    CaptureSession capture{};
    capture.Start();

    ReverseCaptureSession second{};
    second.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"multi-track.mid")).get();

    MidiSequenceTrackRouting routing{};
    routing.Connection(second.Sender);

    player.SetTrackRouting(ChannelOneTrack, routing);

    player.Play();

    VERIFY_IS_TRUE(WaitForMessages(capture, 0,
        [](auto const& messages, size_t)
        {
            for (auto const& message : messages)
            {
                if (IsNoteOnWithVelocity(message[0])) { return true; }
            }

            return false;
        },
        4000));

    player.Stop();

    // All sound off has to reach the routed endpoint too, not only the player's own.
    auto const reached = [](std::vector<std::array<uint32_t, 4>> const& messages)
        {
            for (auto const& message : messages)
            {
                if (IsControlChange(message[0], 120)) { return true; }
            }

            return false;
        };

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(4000);

    bool secondSilenced{ false };

    while (std::chrono::steady_clock::now() < deadline && !secondSilenced)
    {
        secondSilenced = reached(second.Snapshot());

        if (!secondSilenced) { ::Sleep(PollIntervalMilliseconds); }
    }

    VERIFY_IS_TRUE(reached(capture.Snapshot()));
    VERIFY_IS_TRUE(secondSilenced);

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

namespace
{
    // A note off, or the note on with no velocity that many files use instead.
    bool IsNoteEnd(_In_ uint32_t const word)
    {
        auto const messageType = (word >> 28) & 0xF;
        auto const status = (word >> 20) & 0xF;
        auto const velocity = word & 0x7F;

        return messageType == 0x2 && (status == 0x8 || (status == 0x9 && velocity == 0));
    }

    uint8_t KeyOf(_In_ uint32_t const word)
    {
        return static_cast<uint8_t>((word >> 8) & 0x7F);
    }
}

void MidiSequencePlayerTests::PlaybackRateStartsAtOneAndIsClamped()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    VERIFY_ARE_EQUAL(1.0, player.PlaybackRate());
    VERIFY_ARE_EQUAL(0.25, player.MinimumPlaybackRate());
    VERIFY_ARE_EQUAL(2.0, player.MaximumPlaybackRate());

    player.PlaybackRate(0.5);
    VERIFY_ARE_EQUAL(0.5, player.PlaybackRate());

    player.PlaybackRate(10.0);
    VERIFY_ARE_EQUAL(player.MaximumPlaybackRate(), player.PlaybackRate());

    player.PlaybackRate(0.0);
    VERIFY_ARE_EQUAL(player.MinimumPlaybackRate(), player.PlaybackRate());

    player.PlaybackRate(0.75);

    // Something that is not a number is a defect in the caller, not a speed. It is refused, and
    // the rate already set is kept.
    auto refused = false;

    try
    {
        player.PlaybackRate(std::numeric_limits<double>::quiet_NaN());
    }
    catch (winrt::hresult_error const& error)
    {
        refused = error.code() == E_INVALIDARG;
    }

    VERIFY_IS_TRUE(refused);
    VERIFY_ARE_EQUAL(0.75, player.PlaybackRate());

    // The rate belongs to the player rather than to a sequence, so loading one leaves it alone.
    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    VERIFY_ARE_EQUAL(0.75, player.PlaybackRate());

    player.Close();
}

void MidiSequencePlayerTests::PlaysFasterAndSlowerInTheSequencesOwnTime()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    // Two seconds of music at double speed takes about one.
    player.PlaybackRate(2.0);

    auto const started = std::chrono::steady_clock::now();

    player.Play();

    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 6000));

    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();

    LOG_OUTPUT(L"Two seconds of music at double speed took %d ms", static_cast<int>(elapsed));

    // Well short of the two seconds it takes as written, and not impossibly quick.
    VERIFY_IS_TRUE(elapsed >= 900);
    VERIFY_IS_TRUE(elapsed < 1700);

    player.PlaybackRate(0.5);
    player.Play();

    ::Sleep(800);

    auto const slow = player.Position();

    LOG_OUTPUT(L"After 800 ms at half speed the position is %d ms", static_cast<int>(slow.Microseconds / 1000));

    // About 370 ms: the 800 less the lead before the first message, halved. As written it would
    // be past 700. The length and the tempo are still the sequence's own.
    VERIFY_IS_TRUE(slow.Microseconds >= 250000);
    VERIFY_IS_TRUE(slow.Microseconds <= 550000);
    VERIFY_ARE_EQUAL(uint64_t{ 2000000 }, slow.DurationMicroseconds);
    VERIFY_ARE_EQUAL(120.0, slow.BeatsPerMinute);

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::ChangingTheRateWhilePlayingKeepsEveryNoteInOrder()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    player.Play();

    // Swing from the slowest speed to the fastest and back, more often than the player hands
    // messages over. That is the hard case: something handed over at the old speed is nearly
    // always still waiting when the new one is asked for.
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    auto fast = false;
    auto changes = 0;

    while (player.State() == MidiSequencePlayerState::Playing && std::chrono::steady_clock::now() < deadline)
    {
        player.PlaybackRate(fast ? player.MaximumPlaybackRate() : player.MinimumPlaybackRate());

        fast = !fast;
        ++changes;

        ::Sleep(70);
    }

    LOG_OUTPUT(L"Changed speed %d times while playing", changes);

    VERIFY_IS_TRUE(WaitForState(player, MidiSequencePlayerState::Stopped, 6000));

    WaitForMessages(capture, 0,
        [](auto const& messages, size_t)
        {
            size_t ends{ 0 };

            for (auto const& message : messages)
            {
                if (IsNoteEnd(message[0])) { ++ends; }
            }

            return ends >= 8;
        },
        3000);

    auto const messages = capture.Snapshot();

    // The scale plays each key once, upward, so every key has to start before it ends, and the
    // starts have to come in order.
    std::array<bool, 128> started{};
    std::vector<uint8_t> startOrder{};
    size_t endsBeforeStarts{ 0 };

    for (auto const& message : messages)
    {
        auto const word = message[0];

        if (IsNoteOnWithVelocity(word))
        {
            started[KeyOf(word)] = true;
            startOrder.push_back(KeyOf(word));
        }
        else if (IsNoteEnd(word) && !started[KeyOf(word)])
        {
            ++endsBeforeStarts;
        }
    }

    VERIFY_IS_TRUE(changes >= 4);
    VERIFY_ARE_EQUAL(size_t{ 8 }, startOrder.size());
    VERIFY_ARE_EQUAL(size_t{ 0 }, endsBeforeStarts);
    VERIFY_IS_TRUE(std::is_sorted(startOrder.begin(), startOrder.end()));

    player.Close();
}

namespace
{
    // Enough of a Standard MIDI File to put notes exactly where a test needs them. Format 1, 480
    // ticks a quarter note and no tempo event, so the tempo is the default 120 beats per minute
    // and 960 ticks is one second.
    constexpr uint32_t TicksPerSecond = 960;

    struct FileEvent
    {
        uint32_t Tick{ 0 };
        std::vector<uint8_t> Bytes{};
    };

    void AppendVariableLength(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t value)
    {
        std::array<uint8_t, 5> reversed{};
        size_t count{ 0 };

        reversed[count++] = static_cast<uint8_t>(value & 0x7F);

        while ((value >>= 7) != 0)
        {
            reversed[count++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
        }

        while (count > 0)
        {
            bytes.push_back(reversed[--count]);
        }
    }

    MidiSequence ReadSequenceFromTracks(_In_ std::vector<std::vector<FileEvent>> tracks)
    {
        std::vector<uint8_t> bytes{
            'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, static_cast<uint8_t>(tracks.size()), 0x01, 0xE0 };

        for (auto& track : tracks)
        {
            std::stable_sort(track.begin(), track.end(),
                [](FileEvent const& left, FileEvent const& right) { return left.Tick < right.Tick; });

            std::vector<uint8_t> body{};
            uint32_t previousTick{ 0 };

            for (auto const& event : track)
            {
                AppendVariableLength(body, event.Tick - previousTick);
                previousTick = event.Tick;

                body.insert(body.end(), event.Bytes.begin(), event.Bytes.end());
            }

            // end of track
            AppendVariableLength(body, 0);
            body.insert(body.end(), { 0xFF, 0x2F, 0x00 });

            auto const size = static_cast<uint32_t>(body.size());

            bytes.insert(bytes.end(), { 'M', 'T', 'r', 'k' });
            bytes.push_back(static_cast<uint8_t>(size >> 24));
            bytes.push_back(static_cast<uint8_t>(size >> 16));
            bytes.push_back(static_cast<uint8_t>(size >> 8));
            bytes.push_back(static_cast<uint8_t>(size));

            bytes.insert(bytes.end(), body.begin(), body.end());
        }

        auto const result = ReadBytes(bytes);

        VERIFY_IS_TRUE(result != nullptr);
        VERIFY_IS_TRUE(result.Succeeded());

        return result.Sequence();
    }

    // Both halves of a note, the way a file stores them.
    void AddFileNote(
        _Inout_ std::vector<FileEvent>& track,
        _In_ uint8_t const channel,
        _In_ uint8_t const key,
        _In_ uint8_t const velocity,
        _In_ uint32_t const startTick,
        _In_ uint32_t const endTick)
    {
        track.push_back(FileEvent{ startTick, { static_cast<uint8_t>(0x90 | channel), key, velocity } });
        track.push_back(FileEvent{ endTick, { static_cast<uint8_t>(0x80 | channel), key, 0 } });
    }

    // An organ on one channel: a program change, a note held for four seconds the way a stop is
    // held on a control channel, and a short note that is over after a quarter of a second.
    constexpr uint8_t OrganProgram = 19;
    constexpr uint8_t HeldKey = 60;
    constexpr uint8_t HeldVelocity = 100;
    constexpr uint8_t ShortKey = 64;
    constexpr uint32_t ShortEndTick = 240;

    MidiSequence HeldNoteSequence()
    {
        std::vector<FileEvent> track{};

        track.push_back(FileEvent{ 0, { 0xC0, OrganProgram } });

        AddFileNote(track, 0, HeldKey, HeldVelocity, 0, 4 * TicksPerSecond);
        AddFileNote(track, 0, ShortKey, 80, 0, ShortEndTick);

        return ReadSequenceFromTracks({ track });
    }

    // Two tracks, each holding one note for four seconds on a channel of its own.
    constexpr uint8_t SecondTrackKey = 67;

    MidiSequence TwoHeldNotesSequence()
    {
        std::vector<FileEvent> first{};
        AddFileNote(first, 0, HeldKey, HeldVelocity, 0, 4 * TicksPerSecond);

        std::vector<FileEvent> second{};
        AddFileNote(second, 1, SecondTrackKey, 90, 0, 4 * TicksPerSecond);

        return ReadSequenceFromTracks({ first, second });
    }

    // Starts exactly where SeekingWhilePlayingLetsTheNewPositionSound lands.
    constexpr uint8_t LaterKey = 72;

    bool IsProgramChange(_In_ uint32_t const word, _In_ uint8_t const program)
    {
        auto const messageType = (word >> 28) & 0xF;
        auto const status = (word >> 20) & 0xF;

        return messageType == 0x2 && status == 0xC && ((word >> 8) & 0x7F) == program;
    }

    bool IsNoteOnFor(_In_ uint32_t const word, _In_ uint8_t const key)
    {
        return IsNoteOnWithVelocity(word) && KeyOf(word) == key;
    }

    // What would cut a note off: its own end, all notes off or all sound off.
    bool SilencesKey(_In_ uint32_t const word, _In_ uint8_t const key)
    {
        return (IsNoteEnd(word) && KeyOf(word) == key) || IsControlChange(word, 123) || IsControlChange(word, 120);
    }

    // Index of the first message at or after from that matches, or the count when none does.
    size_t FindFrom(
        _In_ std::vector<std::array<uint32_t, 4>> const& messages,
        _In_ size_t const from,
        _In_ std::function<bool(uint32_t)> const& matches)
    {
        for (size_t index = from; index < messages.size(); ++index)
        {
            if (matches(messages[index][0]))
            {
                return index;
            }
        }

        return messages.size();
    }

    bool WaitForNoteOn(
        _In_ CaptureSession& capture,
        _In_ size_t const from,
        _In_ uint8_t const key,
        _In_ uint32_t const timeoutMilliseconds)
    {
        return WaitForMessages(capture, from,
            [key](auto const& messages, size_t start)
            {
                return FindFrom(messages, start, [key](uint32_t word) { return IsNoteOnFor(word, key); }) < messages.size();
            },
            timeoutMilliseconds);
    }

    MidiSequenceChaseOptions const ChannelSettingsAndHeldNotes =
        MidiSequenceChaseOptions::ChannelSettings | MidiSequenceChaseOptions::HeldNotes;
}

void MidiSequencePlayerTests::ChaseOptionsStartAtChannelSettings()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    // What a player has always done, so an application that never asks gets no surprise.
    VERIFY_IS_TRUE(player.ChaseOptions() == MidiSequenceChaseOptions::ChannelSettings);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    VERIFY_IS_TRUE(player.ChaseOptions() == ChannelSettingsAndHeldNotes);

    player.ChaseOptions(MidiSequenceChaseOptions::None);
    VERIFY_IS_TRUE(player.ChaseOptions() == MidiSequenceChaseOptions::None);

    // A bit this version does not know is dropped rather than kept, and reading it back says so.
    player.ChaseOptions(static_cast<MidiSequenceChaseOptions>(0x80000000u) | MidiSequenceChaseOptions::HeldNotes);
    VERIFY_IS_TRUE(player.ChaseOptions() == MidiSequenceChaseOptions::HeldNotes);

    // It belongs to the player rather than to a sequence, so loading one leaves it alone.
    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();
    VERIFY_IS_TRUE(player.ChaseOptions() == MidiSequenceChaseOptions::HeldNotes);

    player.Close();
}

void MidiSequencePlayerTests::ResumingRestartsHeldNotesWhenAsked()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    player.SetSequenceAsync(HeldNoteSequence()).get();

    player.Play();

    // Past the end of the short note, and well inside the held one.
    ::Sleep(600);
    player.Pause();

    // Long enough for the second pass of the pause's silence to have landed.
    ::Sleep(500);

    auto const beforeResume = capture.Count();

    player.Play();

    VERIFY_IS_TRUE(WaitForNoteOn(capture, beforeResume, HeldKey, 2000));

    // The chase goes out in one burst, so anything else it sent is in by now.
    ::Sleep(100);

    auto const messages = capture.Snapshot();

    auto const program = FindFrom(messages, beforeResume, [](uint32_t word) { return IsProgramChange(word, OrganProgram); });
    auto const held = FindFrom(messages, beforeResume, [](uint32_t word) { return IsNoteOnFor(word, HeldKey); });
    auto const shortNote = FindFrom(messages, beforeResume, [](uint32_t word) { return IsNoteOnFor(word, ShortKey); });

    LOG_OUTPUT(L"after resuming: program at %d, held note at %d, of %d messages",
        static_cast<int>(program), static_cast<int>(held), static_cast<int>(messages.size()));

    // The sound first, then the note, or the note starts on whatever sound was there before.
    VERIFY_IS_TRUE(program < held);
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(HeldVelocity), messages[held][0] & 0x7F);
    VERIFY_ARE_EQUAL(uint8_t{ 0 }, ChannelOf(messages[held][0]));

    // A note that was already over stays over.
    VERIFY_ARE_EQUAL(messages.size(), shortNote);

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::HeldNotesAreNotRestartedByDefault()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.SetSequenceAsync(HeldNoteSequence()).get();

    player.Play();
    ::Sleep(600);
    player.Pause();
    ::Sleep(500);

    auto const beforeResume = capture.Count();

    player.Play();

    // The channel settings still go out, as they always have.
    VERIFY_IS_TRUE(WaitForMessages(capture, beforeResume,
        [](auto const& messages, size_t from)
        {
            return FindFrom(messages, from, [](uint32_t word) { return IsProgramChange(word, OrganProgram); }) < messages.size();
        },
        2000));

    // Time enough for a restarted note to have turned up, had there been one.
    ::Sleep(300);

    auto const messages = capture.Snapshot();

    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, beforeResume, [](uint32_t word) { return IsNoteOnFor(word, HeldKey); }));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::ANoteEndingAtTheStartingPointIsNotRestarted()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    player.SetSequenceAsync(HeldNoteSequence()).get();

    // Exactly where the short note ends. It is over there, so restarting it would only be a blip.
    player.SeekToTick(ShortEndTick);
    player.Play();

    VERIFY_IS_TRUE(WaitForNoteOn(capture, 0, HeldKey, 2000));
    ::Sleep(100);

    auto const messages = capture.Snapshot();

    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, 0, [](uint32_t word) { return IsNoteOnFor(word, ShortKey); }));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::MutedTracksDoNotRestartTheirHeldNotes()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    player.SetSequenceAsync(TwoHeldNotesSequence()).get();

    player.SetTrackMuted(1, true);

    player.SeekToMicroseconds(1000000);
    player.Play();

    VERIFY_IS_TRUE(WaitForNoteOn(capture, 0, HeldKey, 2000));
    ::Sleep(100);

    auto const messages = capture.Snapshot();

    // A muted track's note never reached the instrument, so there is nothing of it to restart.
    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, 0, [](uint32_t word) { return IsNoteOnFor(word, SecondTrackKey); }));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::MutingWithHeldNotesKeepsTheOtherTracksSounding()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    player.SetSequenceAsync(TwoHeldNotesSequence()).get();

    player.Play();

    VERIFY_IS_TRUE(WaitForNoteOn(capture, 0, SecondTrackKey, 2000));
    ::Sleep(300);

    // Muting silences every channel, not only the muted track's, so the other track's held note
    // has to come back after that silence.
    auto const beforeMute = capture.Count();

    player.SetTrackMuted(1, true);

    VERIFY_IS_TRUE(WaitForNoteOn(capture, beforeMute, HeldKey, 2000));
    ::Sleep(100);

    auto messages = capture.Snapshot();

    auto const silence = FindFrom(messages, beforeMute, [](uint32_t word) { return IsControlChange(word, 123); });
    auto const restarted = FindFrom(messages, beforeMute, [](uint32_t word) { return IsNoteOnFor(word, HeldKey); });

    VERIFY_IS_TRUE(silence < restarted);
    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, beforeMute, [](uint32_t word) { return IsNoteOnFor(word, SecondTrackKey); }));

    // Unmuting brings back only that track's note. Nothing was silenced this time, so the other
    // note is still sounding and is left alone.
    auto const beforeUnmute = capture.Count();

    player.SetTrackMuted(1, false);

    VERIFY_IS_TRUE(WaitForNoteOn(capture, beforeUnmute, SecondTrackKey, 2000));
    ::Sleep(100);

    messages = capture.Snapshot();

    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, beforeUnmute, [](uint32_t word) { return IsNoteOnFor(word, HeldKey); }));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::SeekingWhilePlayingLetsTheNewPositionSound()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    // A short note at the start, and a long one that starts exactly where the seek lands.
    std::vector<FileEvent> track{};
    AddFileNote(track, 0, ShortKey, 80, 0, ShortEndTick);
    AddFileNote(track, 0, LaterKey, 90, TicksPerSecond, 3 * TicksPerSecond);

    player.SetSequenceAsync(ReadSequenceFromTracks({ track })).get();

    player.Play();
    ::Sleep(300);

    auto const beforeSeek = capture.Count();

    player.SeekToMicroseconds(1000000);

    VERIFY_IS_TRUE(WaitForNoteOn(capture, beforeSeek, LaterKey, 2000));

    // Well past where the second pass of the seek's silence lands.
    ::Sleep(600);

    auto const messages = capture.Snapshot();

    auto const started = FindFrom(messages, beforeSeek, [](uint32_t word) { return IsNoteOnFor(word, LaterKey); });

    // The silence has to come before the music, never on top of it.
    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, started + 1, [](uint32_t word) { return SilencesKey(word, LaterKey); }));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::SeekingWhilePlayingKeepsRestartedNotesSounding()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    player.SetSequenceAsync(HeldNoteSequence()).get();

    player.Play();
    ::Sleep(400);

    auto const beforeSeek = capture.Count();

    // Still inside the held note, which was sounding when the seek silenced everything.
    player.SeekToMicroseconds(1500000);

    VERIFY_IS_TRUE(WaitForNoteOn(capture, beforeSeek, HeldKey, 2000));
    ::Sleep(600);

    auto const messages = capture.Snapshot();

    auto const restarted = FindFrom(messages, beforeSeek, [](uint32_t word) { return IsNoteOnFor(word, HeldKey); });

    // The second pass of the seek's silence turns off what was sounding before the seek, which
    // includes this very note. It has to land before the restart, not after it.
    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, restarted + 1, [](uint32_t word) { return SilencesKey(word, HeldKey); }));

    player.Stop();
    player.Close();
}

namespace
{
    // Sent on channel 1 once every note in a transposition fixture has ended, and long before the
    // file does. What arrives before it is what the sequence played. The silence at the end of
    // the file comes after it, so a note that silence had to turn off is still counted as left
    // sounding.
    constexpr uint8_t FinishedController = 102;

    bool IsFinishedMarker(_In_ uint32_t const word)
    {
        return IsControlChange(word, FinishedController);
    }

    void AddFinishedMarker(_Inout_ std::vector<FileEvent>& track, _In_ uint32_t const tick)
    {
        track.push_back(FileEvent{ tick, { 0xB0, FinishedController, 1 } });

        // Something after it, so the file does not end, and silence everything, at the marker.
        track.push_back(FileEvent{ tick + TicksPerSecond / 2, { 0xB0, FinishedController, 0 } });
    }

    struct PlayedNote
    {
        uint8_t Channel{ 0 };
        uint8_t Key{ 0 };
    };

    // Every note start before the marker, in order.
    std::vector<PlayedNote> StartsBeforeMarker(_In_ std::vector<std::array<uint32_t, 4>> const& messages)
    {
        std::vector<PlayedNote> starts{};

        for (auto const& message : messages)
        {
            auto const word = message[0];

            if (IsFinishedMarker(word))
            {
                break;
            }

            if (IsNoteOnWithVelocity(word))
            {
                starts.push_back(PlayedNote{ ChannelOf(word), KeyOf(word) });
            }
        }

        return starts;
    }

    // How many starts on each channel and key had no end by the marker. Ends with no start are
    // harmless and are not counted.
    size_t NotesLeftSoundingAtMarker(_In_ std::vector<std::array<uint32_t, 4>> const& messages)
    {
        std::array<std::array<uint32_t, 128>, 16> sounding{};

        for (auto const& message : messages)
        {
            auto const word = message[0];

            if (IsFinishedMarker(word))
            {
                break;
            }

            auto& count = sounding[ChannelOf(word)][KeyOf(word)];

            if (IsNoteOnWithVelocity(word))
            {
                ++count;
            }
            else if (IsNoteEnd(word) && count > 0)
            {
                --count;
            }
        }

        size_t left{ 0 };

        for (uint8_t channel = 0; channel < 16; ++channel)
        {
            for (uint8_t key = 0; key < 128; ++key)
            {
                if (sounding[channel][key] != 0)
                {
                    LOG_OUTPUT(L"channel %d key %d: %d left sounding", channel, key, static_cast<int>(sounding[channel][key]));
                    left += sounding[channel][key];
                }
            }
        }

        return left;
    }

    bool WaitForFinishedMarker(_In_ CaptureSession& capture, _In_ uint32_t const timeoutMilliseconds)
    {
        return WaitForMessages(capture, 0,
            [](auto const& messages, size_t start)
            {
                return FindFrom(messages, start, [](uint32_t word) { return IsFinishedMarker(word); }) < messages.size();
            },
            timeoutMilliseconds);
    }

    constexpr uint8_t DrumChannel = 9;
    constexpr uint8_t BassDrum = 36;
    constexpr uint8_t Snare = 38;
}

void MidiSequencePlayerTests::TransposeStartsAtZeroAndIsClamped()
{
    CaptureSession capture{};
    capture.Start(false);

    auto player = MakeBorrowingPlayer(capture);

    VERIFY_ARE_EQUAL(0, player.TransposeSemitones());
    VERIFY_ARE_EQUAL(-12, player.MinimumTransposeSemitones());
    VERIFY_ARE_EQUAL(12, player.MaximumTransposeSemitones());

    player.TransposeSemitones(5);
    VERIFY_ARE_EQUAL(5, player.TransposeSemitones());

    player.TransposeSemitones(40);
    VERIFY_ARE_EQUAL(player.MaximumTransposeSemitones(), player.TransposeSemitones());

    player.TransposeSemitones(-40);
    VERIFY_ARE_EQUAL(player.MinimumTransposeSemitones(), player.TransposeSemitones());

    player.TransposeSemitones(-3);

    // It belongs to the player rather than to a sequence, so loading one leaves it alone.
    player.SetSequenceAsync(ReadTestSequence(L"plain-scale.mid")).get();

    VERIFY_ARE_EQUAL(-3, player.TransposeSemitones());

    player.Close();
}

void MidiSequencePlayerTests::TransposesNotesButNotDrums()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    std::vector<FileEvent> track{};

    // Channel 11 made a GS rhythm part: block A, value 1, with its checksum.
    track.push_back(FileEvent{ 0, { 0xF0, 0x0A, 0x41, 0x10, 0x42, 0x12, 0x40, 0x1A, 0x15, 0x01, 0x10, 0xF7 } });

    // Channel 3 switched to an XG drum kit bank.
    track.push_back(FileEvent{ 0, { 0xB2, 0x00, 0x7F } });

    // One short note at a time, so the order they arrive in is the order they were written.
    AddFileNote(track, 0, 60, 100, 0, 120);
    AddFileNote(track, DrumChannel, BassDrum, 100, 240, 360);
    AddFileNote(track, 2, 40, 100, 480, 600);
    AddFileNote(track, 10, Snare, 100, 720, 840);
    AddFileNote(track, 3, 67, 100, 960, 1080);
    AddFileNote(track, 0, 64, 100, 1200, 1320);

    AddFinishedMarker(track, 1440);

    player.TransposeSemitones(3);
    player.SetSequenceAsync(ReadSequenceFromTracks({ track })).get();

    player.Play();

    VERIFY_IS_TRUE(WaitForFinishedMarker(capture, 6000));

    auto const messages = capture.Snapshot();
    auto const starts = StartsBeforeMarker(messages);

    std::vector<PlayedNote> const expected{
        { 0, 63 },
        { DrumChannel, BassDrum },
        { 2, 40 },
        { 10, Snare },
        { 3, 70 },
        { 0, 67 } };

    VERIFY_ARE_EQUAL(expected.size(), starts.size());

    for (size_t index = 0; index < expected.size() && index < starts.size(); ++index)
    {
        LOG_OUTPUT(L"start %d: channel %d key %d", static_cast<int>(index), starts[index].Channel, starts[index].Key);

        VERIFY_ARE_EQUAL(expected[index].Channel, starts[index].Channel);
        VERIFY_ARE_EQUAL(expected[index].Key, starts[index].Key);
    }

    // Each end went to the note that started, not to the key the file wrote.
    VERIFY_ARE_EQUAL(size_t{ 0 }, NotesLeftSoundingAtMarker(messages));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::ChangingTheKeyWhilePlayingEndsEveryNote()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    std::vector<FileEvent> track{};

    // Chords held for a second, a long note across them, and one key struck again before its
    // first strike has ended. Drums keep time underneath.
    AddFileNote(track, 0, 60, 90, 0, 960);
    AddFileNote(track, 0, 64, 90, 0, 960);
    AddFileNote(track, 0, 67, 90, 0, 960);
    AddFileNote(track, 0, 72, 90, 480, 1440);
    AddFileNote(track, 0, 62, 90, 960, 1920);
    AddFileNote(track, 0, 65, 90, 960, 1920);
    AddFileNote(track, 0, 69, 90, 960, 1920);
    AddFileNote(track, 0, 60, 90, 1152, 1728);
    AddFileNote(track, 0, 60, 90, 1536, 2304);

    for (uint32_t tick = 0; tick < 2400; tick += 240)
    {
        AddFileNote(track, DrumChannel, BassDrum, 100, tick, tick + 60);
    }

    AddFinishedMarker(track, 2880);

    // Set before playing, so the first chord is a known distance from where it was written.
    player.TransposeSemitones(1);
    player.SetSequenceAsync(ReadSequenceFromTracks({ track })).get();

    player.Play();

    // Faster than the player hands messages over, so notes that started in one key are still
    // waiting to end when another is chosen. That is the case that leaves notes hanging.
    std::array<int32_t, 6> const keys{ -7, 12, -12, 0, 3, 1 };

    auto const deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3200);
    size_t changes{ 0 };

    while (player.State() == MidiSequencePlayerState::Playing && std::chrono::steady_clock::now() < deadline)
    {
        ::Sleep(90);

        player.TransposeSemitones(keys[changes % keys.size()]);
        ++changes;
    }

    LOG_OUTPUT(L"Changed key %d times while playing", static_cast<int>(changes));

    VERIFY_IS_TRUE(WaitForFinishedMarker(capture, 6000));

    auto const messages = capture.Snapshot();
    auto const starts = StartsBeforeMarker(messages);

    VERIFY_IS_TRUE(changes >= 10);

    // The first chord, a semitone up.
    VERIFY_IS_TRUE(starts.size() >= 3);
    VERIFY_ARE_EQUAL(uint8_t{ 61 }, starts[0].Key);
    VERIFY_ARE_EQUAL(uint8_t{ 65 }, starts[1].Key);
    VERIFY_ARE_EQUAL(uint8_t{ 68 }, starts[2].Key);

    // The drums never moved.
    for (auto const& start : starts)
    {
        if (start.Channel == DrumChannel)
        {
            VERIFY_ARE_EQUAL(BassDrum, start.Key);
        }
    }

    // And nothing was left for the silence at the end to turn off.
    VERIFY_ARE_EQUAL(size_t{ 0 }, NotesLeftSoundingAtMarker(messages));

    player.Stop();
    player.Close();
}

void MidiSequencePlayerTests::RestartedHeldNotesAreTransposed()
{
    CaptureSession capture{};
    capture.Start();

    auto player = MakeBorrowingPlayer(capture);

    player.ChaseOptions(ChannelSettingsAndHeldNotes);
    player.SetSequenceAsync(HeldNoteSequence()).get();

    player.Play();
    ::Sleep(600);
    player.Pause();
    ::Sleep(500);

    player.TransposeSemitones(2);

    auto const beforeResume = capture.Count();
    auto const movedKey = static_cast<uint8_t>(HeldKey + 2);

    player.Play();

    // The held note comes back in the new key.
    VERIFY_IS_TRUE(WaitForNoteOn(capture, beforeResume, movedKey, 2000));
    ::Sleep(100);

    auto const beforeStop = capture.Count();

    player.Stop();

    // And stopping turns off the note that is actually sounding.
    VERIFY_IS_TRUE(WaitForMessages(capture, beforeStop,
        [movedKey](auto const& messages, size_t from)
        {
            return FindFrom(messages, from, [movedKey](uint32_t word) { return IsNoteEnd(word) && KeyOf(word) == movedKey; }) < messages.size();
        },
        2000));

    auto const messages = capture.Snapshot();

    VERIFY_ARE_EQUAL(messages.size(), FindFrom(messages, beforeResume, [](uint32_t word) { return IsNoteOnFor(word, HeldKey); }));

    player.Close();
}
