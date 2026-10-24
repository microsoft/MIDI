// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "RtpMidiAdmissionTests.h"

using namespace RtpMidi;

#define CHECK(condition) VERIFY_IS_TRUE((condition))

#define CHECK_EQ(actual, expected) \
    do { auto const a_ = (actual); auto const e_ = (expected); VERIFY_IS_TRUE(a_ == e_, WEX::Common::String().Format(L"%hs == %hs, got %lld expected %lld", #actual, #expected, static_cast<long long>(a_), static_cast<long long>(e_))); } while (0)

namespace
{
    PeerAddress Address(_In_ uint8_t const last, _In_ uint16_t const port)
    {
        PeerAddress address{};
        address.Family = 4;
        address.Bytes[0] = 10;
        address.Bytes[3] = last;
        address.Port = port;
        return address;
    }

    PeerAddress Loopback(_In_ uint16_t const port)
    {
        PeerAddress address{};
        address.Family = 4;
        address.Bytes[0] = 127;
        address.Bytes[3] = 1;
        address.Port = port;
        return address;
    }

    // Everything the engine sends and reports, so a test can look at its answers
    class RecordingHost : public ISessionHost
    {
    public:
        struct Sent
        {
            bool Control{ false };
            PeerAddress To{};
            AppleMidiCommand Command{};
            bool IsCommand{ false };
            std::vector<uint8_t> Bytes;
        };

        void SendControl(_In_ PeerAddress const& to, _In_ std::vector<uint8_t> const& datagram) override { Record(true, to, datagram); }
        void SendData(_In_ PeerAddress const& to, _In_ std::vector<uint8_t> const& datagram) override { Record(false, to, datagram); }
        void OnMidi(_In_ Participant const&, _In_ uint64_t, _In_ int64_t, _In_ bool, _In_ std::vector<uint8_t> const&) override {}
        void Log(_In_ std::string const&) override {}

        void OnParticipantChanged(_In_ Participant const& participant) override
        {
            Changes.push_back({ participant.RemoteSsrc, participant.State, participant.Reason });
        }

        // answers of one kind sent to one host, on any port
        size_t Count(_In_ AppleMidiCommand const command, _In_ PeerAddress const& to) const
        {
            return static_cast<size_t>(std::count_if(SentDatagrams.begin(), SentDatagrams.end(),
                [&](Sent const& sent) { return sent.IsCommand && sent.Command == command && sent.To.SameHost(to); }));
        }

        size_t Ended() const
        {
            return static_cast<size_t>(std::count_if(Changes.begin(), Changes.end(),
                [](Change const& change) { return change.State == ParticipantState::Ended; }));
        }

        struct Change
        {
            uint32_t Ssrc{ 0 };
            ParticipantState State{};
            EndReason Reason{};
        };

        std::vector<Sent> SentDatagrams;
        std::vector<Change> Changes;

    private:
        void Record(_In_ bool const control, _In_ PeerAddress const& to, _In_ std::vector<uint8_t> const& datagram)
        {
            Sent sent{};
            sent.Control = control;
            sent.To = to;
            sent.IsCommand = TryGetAppleMidiCommand(datagram.data(), datagram.size(), sent.Command);
            sent.Bytes = datagram;
            SentDatagrams.push_back(std::move(sent));
        }
    };

    SessionConfig HostConfig()
    {
        SessionConfig config{};
        config.LocalName = "Windows Host";
        config.Ssrc = 0x11111111;
        config.AcceptInvitations = true;
        config.MaxParticipants = 16;
        return config;
    }

    // a made-up remote: where it invites from, and what it says about itself
    struct Remote
    {
        PeerAddress Control{};
        uint32_t Ssrc{ 0 };
        uint32_t Token{ 0 };
        std::string Name;
    };

    void SendControlInvitation(_Inout_ Session& session, _In_ Remote const& remote, _In_ uint64_t const now)
    {
        auto const datagram = BuildInvitation(AppleMidiCommand::Invitation, remote.Token, remote.Ssrc, remote.Name);
        session.OnDatagram(true, remote.Control, datagram.data(), datagram.size(), now);
    }

    void SendDataInvitation(_Inout_ Session& session, _In_ Remote const& remote, _In_ uint64_t const now)
    {
        auto const datagram = BuildInvitation(AppleMidiCommand::Invitation, remote.Token, remote.Ssrc, remote.Name);
        auto const from = remote.Control.WithPort(static_cast<uint16_t>(remote.Control.Port + 1));
        session.OnDatagram(false, from, datagram.data(), datagram.size(), now);
    }

    bool IsConnected(_In_ Session const& session, _In_ uint32_t const ssrc)
    {
        for (auto const& participant : session.Snapshot())
        {
            if (participant.RemoteSsrc == ssrc && participant.State == ParticipantState::Connected) return true;
        }

        return false;
    }

    // both invitations, as a remote that connects sends them
    bool Connect(_Inout_ Session& session, _In_ Remote const& remote, _In_ uint64_t const now)
    {
        SendControlInvitation(session, remote, now);
        SendDataInvitation(session, remote, now);

        return IsConnected(session, remote.Ssrc);
    }

    void Leave(_Inout_ Session& session, _In_ Remote const& remote, _In_ uint64_t const now)
    {
        auto const datagram = BuildInvitation(AppleMidiCommand::EndSession, remote.Token, remote.Ssrc, std::string{});
        session.OnDatagram(true, remote.Control, datagram.data(), datagram.size(), now);
        session.Tick(now);
    }

    // connects, then says goodbye, the way a device cycling its session would
    bool ConnectAndLeave(_Inout_ Session& session, _In_ Remote const& remote, _In_ uint64_t const now)
    {
        bool const connected = Connect(session, remote, now);
        Leave(session, remote, now);
        return connected;
    }

    uint32_t NextId()
    {
        static uint32_t next{ 0x5000 };
        return ++next;
    }

    Remote MakeRemote(_In_ PeerAddress const& control, _In_ std::string const& name)
    {
        auto const id = NextId();
        return Remote{ control, 0xA0000000 | id, 0xB0000000 | id, name };
    }

    // a session clock reading well away from zero, like a PC that has been up a while
    constexpr uint64_t Start{ 10'000'000 };
}


void RtpMidiAdmissionTests::TestAcceptsFromOneAddressAreLimited()
{
    RecordingHost host;
    Session session(HostConfig(), host, 1);
    auto const& timings = session.Config().Timings;

    uint64_t now{ Start };
    uint32_t accepted{ 0 };
    uint16_t port{ 6000 };

    // the same name, a new SSRC and port every time: a device cycling IN, IN, BY
    for (uint32_t i = 0; i < timings.MaxAcceptsPerSourcePerWindow + 3; i++)
    {
        if (ConnectAndLeave(session, MakeRemote(Address(5, port), "Churn"), now)) accepted++;

        port += 2;
        now += 100;
    }

    CHECK_EQ(accepted, timings.MaxAcceptsPerSourcePerWindow);
    CHECK(session.Stats().InvitationsLimited >= 3);

    CHECK(ConnectAndLeave(session, MakeRemote(Address(6, 6000), "Neighbor"), now));

    now += timings.AcceptWindow;
    CHECK(ConnectAndLeave(session, MakeRemote(Address(5, 7000), "Churn"), now));
}


void RtpMidiAdmissionTests::TestNewNamesFromOneAddressAreLimited()
{
    RecordingHost host;
    Session session(HostConfig(), host, 2);
    auto const& timings = session.Config().Timings;

    uint64_t now{ Start };
    uint16_t port{ 6000 };

    auto const next = [&](std::string const& name)
        {
            port += 2;
            now += 100;
            return ConnectAndLeave(session, MakeRemote(Address(7, port), name), now);
        };

    for (uint32_t i = 0; i < timings.MaxNamesPerSourcePerNameWindow; i++)
    {
        CHECK(next("Session " + std::to_string(i)));
    }

    CHECK(!next("Made Up Name"));

    // a name this address used before is a reconnect, so it still gets in
    CHECK(next("Session 3"));

    now += timings.NameWindow;
    CHECK(next("Made Up Name"));
}


void RtpMidiAdmissionTests::TestNewNamesOverallAreLimited()
{
    RecordingHost host;
    Session session(HostConfig(), host, 3);
    auto const& timings = session.Config().Timings;

    uint64_t now{ Start };

    // one name from each of many addresses, as forged source addresses could send them
    for (uint32_t i = 0; i < timings.MaxNewNamesPerNameWindow; i++)
    {
        now += 100;
        CHECK(ConnectAndLeave(session, MakeRemote(Address(static_cast<uint8_t>(i + 1), 6000), "Device " + std::to_string(i)), now));
    }

    now += 100;
    CHECK(!ConnectAndLeave(session, MakeRemote(Address(200, 6000), "Never Seen"), now));

    // a device accepted before is not a new name, from wherever it comes
    now += 100;
    CHECK(ConnectAndLeave(session, MakeRemote(Address(201, 6000), "Device 5"), now));

    now += timings.NameWindow;
    CHECK(ConnectAndLeave(session, MakeRemote(Address(200, 6002), "Never Seen"), now));
}


void RtpMidiAdmissionTests::TestAcceptsOverallAreLimited()
{
    RecordingHost host;
    Session session(HostConfig(), host, 4);
    auto const& timings = session.Config().Timings;

    uint64_t now{ Start };

    for (uint32_t i = 0; i < timings.MaxAcceptsPerWindow; i++)
    {
        now += 100;
        CHECK(ConnectAndLeave(session, MakeRemote(Address(static_cast<uint8_t>(i + 1), 6000), "Rack " + std::to_string(i % 10)), now));
    }

    now += 100;
    CHECK(!ConnectAndLeave(session, MakeRemote(Address(250, 6000), "Rack 1"), now));

    now += timings.AcceptWindow;
    CHECK(ConnectAndLeave(session, MakeRemote(Address(250, 6002), "Rack 1"), now));
}


void RtpMidiAdmissionTests::TestHalfOpenInvitationsFromOneAddressAreLimited()
{
    RecordingHost host;
    Session session(HostConfig(), host, 5);
    auto const& timings = session.Config().Timings;

    uint64_t const now{ Start };

    std::vector<Remote> remotes;
    for (uint32_t i = 0; i <= timings.MaxHalfOpenPerSource; i++)
    {
        remotes.push_back(MakeRemote(Address(9, static_cast<uint16_t>(6000 + 2 * i)), "Half Open " + std::to_string(i)));
        SendControlInvitation(session, remotes.back(), now);
    }

    CHECK_EQ(host.Count(AppleMidiCommand::InvitationAccepted, Address(9, 0)), static_cast<size_t>(timings.MaxHalfOpenPerSource));
    CHECK_EQ(host.Count(AppleMidiCommand::InvitationRejected, Address(9, 0)), 1u);

    // another address is not held back by this one
    CHECK(Connect(session, MakeRemote(Address(10, 6000), "Elsewhere"), now));

    // one finishes, which makes room for the one turned away
    SendDataInvitation(session, remotes.front(), now);
    CHECK(IsConnected(session, remotes.front().Ssrc));
    CHECK(Connect(session, remotes.back(), now));
}


void RtpMidiAdmissionTests::TestLoopbackIsNotLimited()
{
    RecordingHost host;
    Session session(HostConfig(), host, 6);
    auto const& timings = session.Config().Timings;

    uint64_t now{ Start };
    uint16_t port{ 6000 };

    // a program on this PC, which only it can send from
    for (uint32_t i = 0; i < timings.MaxAcceptsPerWindow + 10; i++)
    {
        port += 2;
        now += 10;
        CHECK(ConnectAndLeave(session, MakeRemote(Loopback(port), "Local " + std::to_string(i)), now));
    }

    for (uint32_t i = 0; i <= timings.MaxHalfOpenPerSource; i++)
    {
        port += 2;
        SendControlInvitation(session, MakeRemote(Loopback(port), "Waiting " + std::to_string(i)), now);
    }

    CHECK_EQ(host.Count(AppleMidiCommand::InvitationRejected, Loopback(0)), 0u);
    CHECK_EQ(session.Stats().InvitationsLimited, 0u);
}


void RtpMidiAdmissionTests::TestRefusedInvitationEndsNothing()
{
    auto admission = Admission::Accept;

    auto config = HostConfig();
    config.Admit = [&](std::string const&, PeerAddress const&) { return admission; };

    RecordingHost host;
    Session session(config, host, 7);

    uint64_t now{ Start };

    auto const mac = MakeRemote(Address(9, 5004), "Studio Mac");
    CHECK(Connect(session, mac, now));

    // Invitations with its name or its SSRC, from its address, which are turned away. Before, any
    // of them ended its connection first.
    admission = Admission::Reject;
    now += 100;
    SendControlInvitation(session, MakeRemote(Address(9, 6000), "Studio Mac"), now);

    admission = Admission::Hold;
    now += 100;
    SendControlInvitation(session, MakeRemote(Address(9, 6002), "Studio Mac"), now);

    admission = Admission::Reject;
    now += 100;
    SendControlInvitation(session, Remote{ Address(9, 6004), mac.Ssrc, 0x12345, "Someone Else" }, now);

    session.Tick(now);

    CHECK(IsConnected(session, mac.Ssrc));
    CHECK_EQ(host.Ended(), 0u);
}


void RtpMidiAdmissionTests::TestUnfinishedInvitationEndsNothing()
{
    RecordingHost host;
    Session session(HostConfig(), host, 8);

    uint64_t now{ Start };

    auto const mac = MakeRemote(Address(9, 5004), "Studio Mac");
    CHECK(Connect(session, mac, now));

    // accepted on the control port, but the data port invitation never comes
    auto const newcomer = MakeRemote(Address(9, 6000), "Studio Mac");
    now += 100;
    SendControlInvitation(session, newcomer, now);
    CHECK_EQ(host.Count(AppleMidiCommand::InvitationAccepted, Address(9, 0)), 3u);

    now += 1000;
    session.Tick(now);
    CHECK(IsConnected(session, mac.Ssrc));
    CHECK_EQ(host.Ended(), 0u);

    // A restarted device finishes both: only then does it replace its old connection, reported
    // down before the new one is reported up
    SendDataInvitation(session, newcomer, now);
    session.Tick(now);

    CHECK(!IsConnected(session, mac.Ssrc));
    CHECK(IsConnected(session, newcomer.Ssrc));
    CHECK_EQ(session.Snapshot().size(), 1u);

    auto const& changes = host.Changes;
    CHECK(changes.size() >= 2);

    if (changes.size() >= 2)
    {
        auto const& down = changes[changes.size() - 2];
        auto const& up = changes.back();

        CHECK(down.Ssrc == mac.Ssrc && down.State == ParticipantState::Ended && down.Reason == EndReason::Replaced);
        CHECK(up.Ssrc == newcomer.Ssrc && up.State == ParticipantState::Connected);
    }
}


void RtpMidiAdmissionTests::TestInvitationNeverEndsOurOwnSession()
{
    auto config = HostConfig();
    config.AcceptInvitations = false;

    RecordingHost host;
    Session session(config, host, 9);

    uint64_t now{ Start };
    auto const remoteControl = Address(20, 5004);
    uint32_t const remoteSsrc{ 0xCAFE0001 };

    session.Invite(remoteControl, now);

    // the token our invitation carried
    uint32_t token{ 0 };
    for (auto const& sent : host.SentDatagrams)
    {
        if (sent.IsCommand && sent.Command == AppleMidiCommand::Invitation && sent.To == remoteControl)
        {
            if (auto const invitation = ParseInvitation(sent.Bytes.data(), sent.Bytes.size())) token = invitation->InitiatorToken;
        }
    }

    CHECK(token != 0);

    // the remote answers both of our invitations
    auto const accepted = BuildInvitation(AppleMidiCommand::InvitationAccepted, token, remoteSsrc, "Remote Host");
    session.OnDatagram(true, remoteControl, accepted.data(), accepted.size(), now);
    session.OnDatagram(false, remoteControl.WithPort(5005), accepted.data(), accepted.size(), now);

    auto const before = session.Snapshot();
    CHECK_EQ(before.size(), 1u);
    CHECK(!before.empty() && before[0].State == ParticipantState::Synchronizing);

    // An invitation to our control port with the remote's SSRC, from the remote's address. Before,
    // it ended our own session before it was refused.
    now += 100;
    SendControlInvitation(session, Remote{ Address(20, 6000), remoteSsrc, 0x777, "Remote Host" }, now);
    session.Tick(now);

    auto const after = session.Snapshot();
    CHECK_EQ(after.size(), 1u);
    CHECK(!after.empty() && after[0].State != ParticipantState::Ended && after[0].WeInitiated);
    CHECK_EQ(host.Ended(), 0u);
    CHECK_EQ(host.Count(AppleMidiCommand::InvitationRejected, Address(20, 0)), 1u);
}


void RtpMidiAdmissionTests::TestRepeatedInvitationIsAnsweredSparingly()
{
    RecordingHost host;
    Session session(HostConfig(), host, 10);
    auto const& timings = session.Config().Timings;

    uint64_t now{ Start };
    auto const remote = MakeRemote(Address(11, 6000), "Repeater");

    // a burst of the same invitation gets one answer
    for (int i = 0; i < 30; i++) SendControlInvitation(session, remote, now);
    CHECK_EQ(host.Count(AppleMidiCommand::InvitationAccepted, remote.Control), 1u);

    // a remote whose answer was lost asks again later, and is answered, up to its own limit
    for (uint32_t i = 0; i < timings.InvitationMaxAttempts + 5; i++)
    {
        now += timings.AcceptReplyMinInterval;
        SendControlInvitation(session, remote, now);
    }

    CHECK_EQ(host.Count(AppleMidiCommand::InvitationAccepted, remote.Control), static_cast<size_t>(timings.InvitationMaxAttempts));
    CHECK(session.Stats().RepeatedAcceptsSuppressed > 0);

    // still one participant, waiting for its data port invitation
    CHECK_EQ(session.Snapshot().size(), 1u);
}
