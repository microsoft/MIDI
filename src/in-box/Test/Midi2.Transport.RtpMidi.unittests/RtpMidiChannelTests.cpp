// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// What reaches the network for notes the service sends on channel 2. Reported: a pipe organ
// behind rtpmidid also played them on channel 1, and those notes never stopped.
// ============================================================================

#include "pch.h"
#include "RtpMidiTransportTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiTest::Peer;
using RtpMidiTest::WaitFor;

namespace
{
    // removes the test's host however the test ends
    template <typename Action>
    class AtExit
    {
    public:
        explicit AtExit(_In_ Action action) : m_action(std::move(action)) {}
        ~AtExit() { try { m_action(); } catch (...) {} }

        AtExit(AtExit const&) = delete;
        AtExit& operator=(AtExit const&) = delete;

    private:
        Action m_action;
    };

    // the channels one packet names: in its channel voice commands, and in its recovery journal
    struct NamedChannels
    {
        bool Readable{ false };
        bool HasJournal{ false };
        std::vector<uint8_t> InCommands;
        std::vector<uint8_t> InJournal;
    };

    NamedChannels ChannelsNamedBy(_In_ std::vector<uint8_t> const& datagram)
    {
        NamedChannels named{};

        RtpMidi::CommandSectionDecoder decoder;
        RtpMidi::DecodedPacket packet;
        if (decoder.Decode(datagram.data(), datagram.size(), packet) != RtpMidi::DecodeStatus::Ok) return named;

        for (auto const& event : packet.Events)
        {
            if (!event.Bytes.empty() && event.Bytes[0] >= 0x80 && event.Bytes[0] < 0xF0)
            {
                named.InCommands.push_back(static_cast<uint8_t>(event.Bytes[0] & 0x0F));
            }
        }

        named.HasJournal = packet.HasJournal;

        if (packet.HasJournal)
        {
            RtpMidi::RecoveryJournal journal{};
            if (!RtpMidi::ParseRecoveryJournal(datagram.data() + packet.JournalOffset, packet.JournalSize, journal)) return named;

            for (auto const& channel : journal.Channels) named.InJournal.push_back(channel.Channel);
        }

        named.Readable = true;
        return named;
    }
}


// The apps in the report send MIDI 1.0 (through WinMM) and MIDI 2.0 messages, so both go out here.
// The remote confirms nothing, so every journal describes everything sent since the first packet.
void RtpMidiTransportTests::TestNotesKeepTheirChannelOnTheWire()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"Channel Host\"", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_TRUE(FindHost(hostId).GetNamedBoolean(L"sendRecoveryJournal", false), L"a host sends a recovery journal unless told not to");

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("Channel Organ", false);
    remote.WithholdFeedback();
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 81);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    // channel 1 in a UMP is MIDI channel 2
    std::vector<std::vector<uint32_t>> const messages =
    {
        { 0x20913C64 },                 // MIDI 1.0 Note On, note 60, velocity 100
        { 0x20813C40 },                 // MIDI 1.0 Note Off, note 60
        { 0x40913E00, 0x80000000 },     // MIDI 2.0 Note On, note 62, velocity 0x8000
        { 0x40813E00, 0x00000000 },     // MIDI 2.0 Note Off, note 62
    };

    for (size_t i = 0; i < messages.size(); i++)
    {
        VERIFY_SUCCEEDED(opened.Send(messages[i].data(), static_cast<UINT>(messages[i].size() * sizeof(uint32_t))), L"the service sends to the endpoint");

        // one at a time, as a player's keys arrive, so each goes in a packet of its own
        VERIFY_IS_TRUE(WaitFor([&]() { return remote.Received().size() >= (i + 1) * 3; }, 3000), L"the message arrives");
    }

    std::vector<uint8_t> const expected{ 0x91, 0x3C, 0x64, 0x81, 0x3C, 0x40, 0x91, 0x3E, 0x40, 0x81, 0x3E, 0x00 };
    VERIFY_IS_TRUE(remote.Received() == expected, L"the remote gets the four notes on channel 2 and nothing else");

    auto const packets = remote.RtpPackets();
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(packets.size()), static_cast<uint32_t>(messages.size()), L"a packet for each message");

    uint32_t journalsNamingAChannel{ 0 };

    for (auto const& datagram : packets)
    {
        auto const named = ChannelsNamedBy(datagram);

        VERIFY_IS_TRUE(named.Readable, L"the packet decodes, journal included");
        VERIFY_IS_TRUE(named.HasJournal, L"the packet carries a recovery journal");
        VERIFY_ARE_EQUAL(static_cast<uint32_t>(named.InCommands.size()), 1u, L"one command in the packet");

        for (auto const channel : named.InCommands) VERIFY_ARE_EQUAL(static_cast<uint32_t>(channel), 1u, L"the command is on channel 2");
        for (auto const channel : named.InJournal) VERIFY_ARE_EQUAL(static_cast<uint32_t>(channel), 1u, L"the journal names channel 2 and no other");

        if (!named.InJournal.empty()) journalsNamingAChannel++;
    }

    // the first packet has nothing before it to describe; each later one describes note 60 or 62
    VERIFY_ARE_EQUAL(journalsNamingAChannel, static_cast<uint32_t>(packets.size() - 1), L"every packet after the first has a channel journal");
}


// The way to keep a receiver that mishandles the recovery journal working: a host with the journal
// turned off sends the MIDI list alone, with no journal section at all.
void RtpMidiTransportTests::TestHostWithoutJournalSendsNone()
{
    uint16_t port{ 0 };
    auto const hostId = CreateHost(L"\"name\":\"No Journal Host\",\"sendRecoveryJournal\":false", port);
    AtExit removeHost([&]() { RemoveHost(hostId); });

    VERIFY_IS_FALSE(FindHost(hostId).GetNamedBoolean(L"sendRecoveryJournal", true), L"the host reports the journal off");

    auto const baseline = m_deviceManager->Endpoints().size();

    Peer remote("No Journal Organ", false);
    VERIFY_IS_TRUE(remote.Start(), L"the remote binds a loopback port pair");
    remote.Invite(port);

    VERIFY_IS_TRUE(WaitFor([&]() { return m_deviceManager->Endpoints().size() == baseline + 1; }, 5000), L"the remote gets an endpoint");

    RtpMidiTest::OpenedEndpoint opened(m_transport, m_deviceManager->Endpoints()[baseline].InterfaceId, 82);
    VERIFY_IS_TRUE(opened.IsOpen(), L"the service opens the endpoint");

    uint32_t const noteOn{ 0x20913C64 };
    uint32_t const noteOff{ 0x20813C40 };

    VERIFY_SUCCEEDED(opened.Send(&noteOn, sizeof(noteOn)), L"the service sends a Note On");
    VERIFY_IS_TRUE(WaitFor([&]() { return remote.Received().size() >= 3; }, 3000), L"the Note On arrives");

    VERIFY_SUCCEEDED(opened.Send(&noteOff, sizeof(noteOff)), L"the service sends a Note Off");
    VERIFY_IS_TRUE(WaitFor([&]() { return remote.Received().size() >= 6; }, 3000), L"the Note Off arrives");

    VERIFY_IS_TRUE(remote.Received() == (std::vector<uint8_t>{ 0x91, 0x3C, 0x64, 0x81, 0x3C, 0x40 }), L"both notes arrive on channel 2");

    auto const packets = remote.RtpPackets();
    VERIFY_ARE_EQUAL(static_cast<uint32_t>(packets.size()), 2u, L"a packet for each message");

    // 0x03: no journal flag, a 3-byte MIDI list
    std::vector<std::vector<uint8_t>> const payloads{ { 0x03, 0x91, 0x3C, 0x64 }, { 0x03, 0x81, 0x3C, 0x40 } };

    for (size_t i = 0; i < packets.size() && i < payloads.size(); i++)
    {
        RtpMidi::RtpHeader header{};
        VERIFY_IS_TRUE(RtpMidi::ParseRtpHeader(packets[i].data(), packets[i].size(), header), L"the RTP header parses");

        std::vector<uint8_t> const payload(packets[i].begin() + static_cast<std::ptrdiff_t>(header.PayloadOffset), packets[i].end());
        VERIFY_IS_TRUE(payload == payloads[i], L"the payload is the MIDI list alone");
    }
}
