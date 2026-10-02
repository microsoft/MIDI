// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RtpMidiDnssdTests.h"

#include "RtpMidiTestMdns.h"
#include "RtpMidiTestPeer.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using WindowsMidiServicesInternal::BuildDnssdPtrAnnouncements;
using WindowsMidiServicesInternal::MidiDnssdAnnouncementResult;
using WindowsMidiServicesInternal::MidiDnssdFollowUpAnnouncer;
using WindowsMidiServicesInternal::MidiDnssdService;

namespace
{
    RtpMidi::PeerAddress At(std::wstring const& text, uint32_t const scope)
    {
        RtpMidi::PeerAddress address{};
        VERIFY_IS_TRUE(RtpMidiTest::TryParseAddress(text, 5004, address));
        address.ScopeId = scope;
        return address;
    }

    // stands in for the network, and keeps what each repeat named
    class Recorder
    {
    public:
        struct Repeat
        {
            uint64_t Tick{ 0 };
            GUID AdapterId{};
            std::vector<std::string> Records;
        };

        auto Sender()
        {
            return [this](std::vector<std::vector<uint8_t>> const& packets, GUID const& adapterId)
            {
                Repeat repeat{ GetTickCount64(), adapterId, {} };

                for (auto const& packet : packets)
                {
                    RtpMidiTest::Mdns::Message message{};
                    if (!RtpMidiTest::Mdns::Parse(packet.data(), packet.size(), message)) continue;

                    for (auto const& record : message.Records) repeat.Records.push_back(record.Data);
                }

                auto lock = std::scoped_lock{ m_lock };
                m_repeats.push_back(std::move(repeat));

                return MidiDnssdAnnouncementResult{ 1, 1, 0 };
            };
        }

        std::vector<Repeat> Repeats() { auto lock = std::scoped_lock{ m_lock }; return m_repeats; }

    private:
        std::mutex m_lock;
        std::vector<Repeat> m_repeats;
    };

    constexpr wchar_t ServiceType[] = L"_apple-midi._udp.local";
}

void RtpMidiDnssdTests::TestHostNameForAddress()
{
    MidiDnssdService mac{};
    mac.HostName = L"Studio-Mac.local";
    mac.IPv4Addresses = { L"192.168.1.183" };
    mac.IPv6Addresses = { L"fe80::cec:e610:74fd:7d8d" };

    // a second session the same Mac advertises
    MidiDnssdService macSecondSession{};
    macSecondSession.HostName = L"studio-mac.local";
    macSecondSession.IPv4Addresses = { L"192.168.1.183" };

    MidiDnssdService interfaceBox{};
    interfaceBox.HostName = L"Interface.local";
    interfaceBox.IPv4Addresses = { L"192.168.1.50" };

    std::vector<MidiDnssdService> const services{ mac, macSecondSession, interfaceBox };

    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress(services, At(L"192.168.1.183", 0)) == L"Studio-Mac.local", L"IPv4");
    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress(services, At(L"fe80::cec:e610:74fd:7d8d", 21)) == L"Studio-Mac.local",
        L"a link-local IPv6 address with a scope, advertised without one");
    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress(services, At(L"192.168.1.50", 0)) == L"Interface.local", L"another device");
    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress(services, At(L"192.168.1.99", 0)).empty(), L"none for an address nothing advertises");
    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress(services, RtpMidi::PeerAddress{}).empty(), L"none for no address");

    MidiDnssdService stale{};
    stale.HostName = L"Old-Laptop.local";
    stale.IPv4Addresses = { L"192.168.1.183" };

    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress({ mac, stale }, At(L"192.168.1.183", 0)).empty(),
        L"none when two different hosts list the same address");

    // the browser names the adapter a link-local address was seen on
    MidiDnssdService scoped{};
    scoped.HostName = L"Scoped-Mac.local";
    scoped.IPv6Addresses = { L"fe80::cec:e610:74fd:7d8e%21" };

    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress({ scoped }, At(L"fe80::cec:e610:74fd:7d8e", 21)) == L"Scoped-Mac.local",
        L"a link-local address advertised with the same scope");
    VERIFY_IS_TRUE(RtpMidiMdns::FindHostNameForAddress({ scoped }, At(L"fe80::cec:e610:74fd:7d8e", 27)).empty(),
        L"none for the same link-local address on another adapter, which is another device");
}

void RtpMidiDnssdTests::TestAnnouncementPacket()
{
    std::string const macName{ "Pete\xE2\x80\x99s MacBook Pro" };
    auto const packets = BuildDnssdPtrAnnouncements("_apple-midi._udp.local", { "Pete PC", macName }, 4500, 1200);

    VERIFY_ARE_EQUAL(packets.size(), 1u, L"two hosts fit in one packet");

    RtpMidiTest::Mdns::Message message{};
    VERIFY_IS_TRUE(RtpMidiTest::Mdns::Parse(packets[0].data(), packets[0].size(), message), L"parses as mDNS");
    VERIFY_IS_TRUE(message.Id == 0 && message.Flags == 0x8400 && message.IsResponse, L"ID 0, an authoritative response");
    VERIFY_ARE_EQUAL(message.Records.size(), 2u);

    for (auto const& record : message.Records)
    {
        VERIFY_IS_TRUE(record.Section == "an" && record.Type == 12 && record.Name == "_apple-midi._udp.local", L"a shared PTR answer");
        VERIFY_IS_FALSE(record.TopBit, L"no cache-flush bit on a shared record");
        VERIFY_ARE_EQUAL(record.Ttl, 4500u);
    }

    VERIFY_IS_TRUE(message.Records[0].Data == "-> Pete PC._apple-midi._udp.local", L"a record points at its instance");
    VERIFY_IS_TRUE(message.Records[1].Data == "-> " + macName + "._apple-midi._udp.local", L"a UTF-8 name goes out unchanged");
}

void RtpMidiDnssdTests::TestAnnouncementLeavesOutBadLabels()
{
    VERIFY_IS_TRUE(BuildDnssdPtrAnnouncements("_apple-midi._udp.local", { "", "a.b", std::string(64, 'x') }, 4500, 1200).empty(),
        L"empty, dotted and over-long labels are left out");
    VERIFY_IS_TRUE(BuildDnssdPtrAnnouncements("", { "Pete PC" }, 4500, 1200).empty(), L"nothing without a service type");
    VERIFY_IS_TRUE(BuildDnssdPtrAnnouncements("_apple-midi..local", { "Pete PC" }, 4500, 1200).empty(),
        L"nothing for a service type with an empty label");
}

void RtpMidiDnssdTests::TestAnnouncementSplitsAcrossPackets()
{
    std::vector<std::string> many;
    for (int i = 0; i < 40; i++) many.push_back("Host " + std::to_string(i) + std::string(50, 'h'));

    auto const packets = BuildDnssdPtrAnnouncements("_apple-midi._udp.local", many, 4500, 1200);
    VERIFY_IS_TRUE(packets.size() > 1, L"forty hosts need more than one packet");

    size_t records{ 0 };

    for (auto const& packet : packets)
    {
        VERIFY_IS_TRUE(packet.size() <= 1200, L"every packet is under the limit");

        RtpMidiTest::Mdns::Message message{};
        VERIFY_IS_TRUE(RtpMidiTest::Mdns::Parse(packet.data(), packet.size(), message), L"every packet parses");
        records += message.Records.size();
    }

    VERIFY_ARE_EQUAL(records, many.size(), L"no host is lost in the split");
}

void RtpMidiDnssdTests::TestAnnouncerRepeatsTwice()
{
    Recorder recorder;
    MidiDnssdFollowUpAnnouncer announcer;
    VERIFY_ARE_EQUAL(announcer.Start(ServiceType, nullptr, recorder.Sender(), 150, 450), S_OK);

    auto const added = GetTickCount64();
    announcer.AddRegistration(L"Pete PC");

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return recorder.Repeats().size() >= 2; }, 3000), L"two repeats after one registration");

    auto const repeats = recorder.Repeats();
    VERIFY_IS_TRUE(repeats[0].Tick >= added + 150 && repeats[1].Tick >= added + 450, L"neither repeat comes early");
    VERIFY_IS_TRUE(repeats[0].Records == std::vector<std::string>{ "-> Pete PC._apple-midi._udp.local" } && repeats[1].Records == repeats[0].Records,
        L"each repeat names the host");

    Sleep(600);
    VERIFY_ARE_EQUAL(recorder.Repeats().size(), 2u, L"nothing more once both repeats are out");
}

void RtpMidiDnssdTests::TestAnnouncerNeverRepeatsAWithdrawnHost()
{
    Recorder recorder;
    MidiDnssdFollowUpAnnouncer announcer;
    announcer.Start(ServiceType, nullptr, recorder.Sender(), 150, 450);

    announcer.AddRegistration(L"Gone Host");
    announcer.RemoveRegistration(L"gone host");

    Sleep(700);
    VERIFY_IS_TRUE(recorder.Repeats().empty(), L"a withdrawn host is never repeated, matched without regard to case");
}

void RtpMidiDnssdTests::TestAnnouncerTimesABurstOfRegistrations()
{
    Recorder recorder;
    MidiDnssdFollowUpAnnouncer announcer;
    announcer.Start(ServiceType, nullptr, recorder.Sender(), 300, 900);

    auto const firstAdded = GetTickCount64();
    announcer.AddRegistration(L"First");

    Sleep(200);

    auto const secondAdded = GetTickCount64();
    announcer.AddRegistration(L"Second");
    announcer.AddRegistration(L"SECOND");

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return recorder.Repeats().size() >= 2; }, 3000), L"two repeats for a burst of registrations");

    auto const repeats = recorder.Repeats();

    // held back, it would come 300 ms after the second registration instead of about 100
    VERIFY_IS_TRUE(repeats[0].Tick >= firstAdded + 300 && repeats[0].Tick < secondAdded + 200, L"a later registration does not hold back the first repeat");
    VERIFY_IS_TRUE(repeats[1].Tick >= secondAdded + 900, L"the last repeat waits for the last registration");
    VERIFY_IS_TRUE(repeats[0].Records.size() == 2 && repeats[1].Records.size() == 2, L"each repeat names every host, once");
}

void RtpMidiDnssdTests::TestAnnouncerSendsNothingOnceStopped()
{
    Recorder recorder;
    MidiDnssdFollowUpAnnouncer announcer;
    announcer.Start(ServiceType, nullptr, recorder.Sender(), 150, 450);

    announcer.AddRegistration(L"Stopped Host");
    announcer.Stop();
    announcer.AddRegistration(L"Added After Stop");

    Sleep(700);
    VERIFY_IS_TRUE(recorder.Repeats().empty(), L"nothing is sent once it has stopped");
}

void RtpMidiDnssdTests::TestAnnouncerSendsALimitedHostOnlyOnItsAdapter()
{
    // {6B29FC40-CA47-1067-B31D-00DD010662DA}
    GUID const wired{ 0x6b29fc40, 0xca47, 0x1067, { 0xb3, 0x1d, 0x00, 0xdd, 0x01, 0x06, 0x62, 0xda } };

    Recorder recorder;
    MidiDnssdFollowUpAnnouncer announcer;
    announcer.Start(ServiceType, nullptr, recorder.Sender(), 150, 450);

    announcer.AddRegistration(L"Everywhere");
    announcer.AddRegistration(L"Only Wired", wired);

    VERIFY_IS_TRUE(RtpMidiTest::WaitFor([&]() { return recorder.Repeats().size() >= 4; }, 3000), L"two sends in each of the two repeats");

    auto const repeats = recorder.Repeats();

    VERIFY_IS_TRUE(IsEqualGUID(repeats[0].AdapterId, GUID_NULL) &&
        repeats[0].Records == std::vector<std::string>{ "-> Everywhere._apple-midi._udp.local" },
        L"the host on every adapter goes everywhere");
    VERIFY_IS_TRUE(IsEqualGUID(repeats[1].AdapterId, wired) &&
        repeats[1].Records == std::vector<std::string>{ "-> Only Wired._apple-midi._udp.local" },
        L"the limited host only goes to its own adapter");
}
