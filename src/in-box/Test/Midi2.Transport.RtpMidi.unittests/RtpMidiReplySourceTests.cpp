// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RtpMidiReplySourceTests.h"

#include "RtpMidiReplySources.h"
#include "RtpMidiTestPeer.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using RtpMidiNet::LocalAddress;
using RtpMidiNet::ReplySourceTable;

namespace
{
    RtpMidi::PeerAddress Remote(std::wstring const& text, uint16_t const port)
    {
        RtpMidi::PeerAddress address{};
        VERIFY_IS_TRUE(RtpMidiTest::TryParseAddress(text, port, address));
        return address;
    }

    // an address of this PC which a remote sent to
    LocalAddress ReachedAt(std::wstring const& text)
    {
        LocalAddress local{};
        local.InterfaceIndex = 7;

        if (InetPtonW(AF_INET, text.c_str(), &local.IPv4) == 1)
        {
            local.Family = AF_INET;
        }
        else
        {
            VERIFY_ARE_EQUAL(1, InetPtonW(AF_INET6, text.c_str(), &local.IPv6));
            local.Family = AF_INET6;
        }

        return local;
    }

    bool RepliesFrom(ReplySourceTable& table, RtpMidi::PeerAddress const& remote, LocalAddress const& expected)
    {
        LocalAddress local{};
        return table.TryGet(remote, local) && local == expected;
    }
}

void RtpMidiReplySourceTests::TestFollowsTheAddressARemoteReaches()
{
    ReplySourceTable table{ 8 };

    auto const mac = Remote(L"2001:db8::10", 5004);
    auto const stable = ReachedAt(L"2001:db8::2");
    auto const temporary = ReachedAt(L"2001:db8::8f3a");

    table.Remember(mac, stable, 1);
    VERIFY_IS_TRUE(RepliesFrom(table, mac, stable));

    // the same remote and port, now reaching this PC at another of its addresses
    table.Remember(mac, temporary, 2);
    VERIFY_IS_TRUE(RepliesFrom(table, mac, temporary), L"Replies follow the address the remote used last");
    VERIFY_ARE_EQUAL(static_cast<size_t>(1), table.Count());

    LocalAddress unknown{};
    VERIFY_IS_FALSE(table.TryGet(Remote(L"2001:db8::11", 5004), unknown), L"A remote never heard from has no entry");
}

void RtpMidiReplySourceTests::TestFullTableMakesRoomForANewRemote()
{
    ReplySourceTable table{ 4 };

    auto const local = ReachedAt(L"192.168.1.20");

    for (uint16_t port = 1; port <= 4; port++)
    {
        table.Remember(Remote(L"192.168.1.99", port), local, port);
    }

    VERIFY_ARE_EQUAL(static_cast<size_t>(4), table.Count());

    // A full table used to refuse this, so the remote was answered from whatever address
    // Windows picked
    auto const newcomer = Remote(L"192.168.1.50", 5004);
    table.Remember(newcomer, local, 5);

    VERIFY_IS_TRUE(RepliesFrom(table, newcomer, local), L"A new remote gets an entry when the table is full");
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), table.Count(), L"The table stays at its capacity");

    LocalAddress evicted{};
    VERIFY_IS_FALSE(table.TryGet(Remote(L"192.168.1.99", 1), evicted), L"The remote heard from least recently made room");
}

void RtpMidiReplySourceTests::TestRemoteStillTalkingIsNotPushedOut()
{
    ReplySourceTable table{ 4 };

    auto const local = ReachedAt(L"192.168.1.20");
    auto const peer = Remote(L"192.168.1.183", 5004);

    table.Remember(peer, local, 1);

    table.Remember(Remote(L"10.0.0.1", 1000), local, 2);
    table.Remember(Remote(L"10.0.0.2", 1000), local, 3);
    table.Remember(Remote(L"10.0.0.3", 1000), local, 4);

    // the peer is still sending clock sync
    table.Remember(peer, local, 5);

    table.Remember(Remote(L"10.0.0.4", 1000), local, 6);
    table.Remember(Remote(L"10.0.0.5", 1000), local, 7);

    VERIFY_IS_TRUE(RepliesFrom(table, peer, local), L"A remote which is still talking keeps its entry");
    VERIFY_ARE_EQUAL(static_cast<size_t>(4), table.Count());

    LocalAddress evicted{};
    VERIFY_IS_FALSE(table.TryGet(Remote(L"10.0.0.1", 1000), evicted), L"The quiet stranger made room instead");
}
