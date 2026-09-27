// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. RTP-MIDI / AppleMIDI spike for Windows MIDI Services.
//
// Speaks AppleMIDI to real peers (macOS, iOS, rtpMIDI for Windows, iConnectivity) using Winsock
// and the Windows mDNS stack, so the protocol and the discovery plan can be proven before any
// of it becomes a service transport. It does not touch the MIDI service.
//
//   rtpmidi-spike selftest
//   rtpmidi-spike browse   [--seconds N] [--type _apple-midi._udp.local]
//   rtpmidi-spike register [--name LABEL] [--port P] [--seconds N] [--terminate] [--type T] [--txt]
//                          [--follow-up]   (repeats the announcement the way the transport does)
//   rtpmidi-spike register-winrt [--name LABEL] [--type _wmsprobe._udp.local] [--port P] [--seconds N] [--txt]
//   rtpmidi-spike listen   [--name LABEL] [--port P] [--seconds N] [--no-advertise] [--journal] [--send-test] [--echo]
//   rtpmidi-spike connect  <instance label | address:port> [--port P] [--seconds N] [--journal]
//                          [--send-test] [--notes] [--sysex BYTES] [--simulate-loss] [--feedback-low]
//   listen and connect also take [--rtp-log N] (packets to describe) and [--drop-every N]
//   (discard every Nth incoming RTP packet, to exercise recovery from the peer's journal)
//   rtpmidi-spike loopback [--sysex BYTES]
//   rtpmidi-spike mdns-watch [--filter TEXT] [--seconds N] [--queries]
//   rtpmidi-spike transport-test [--dll PATH]   (runs the service transport DLL against mocks)
//   rtpmidi-spike service status | host | connect | remove   (drives the transport in the running
//                          service; options are at the top of spike_service.cpp)
// ============================================================================

#include "spike_common.h"
#include "spike_dnssd.h"
#include "spike_mdns_watch.h"
#include "spike_net.h"

#include "../transport/RtpMidiMdns.h"

#include <map>
#include <set>
#include <thread>

int RunSelfTest();
int RegisterWithWinRt(std::wstring const& fullName, uint16_t port, uint32_t seconds, bool withText);

#ifdef RTP_TRANSPORT_TEST
int RunTransportTest(std::wstring const& dllPath);
#endif

#ifdef RTP_SDK_CHECK
int RunServiceCommand(std::vector<std::wstring> const& arguments);
#endif

using namespace RtpMidi;
using Spike::Print;
using Spike::ToUtf8;
using Spike::ToWide;

namespace
{
    struct Options
    {
        std::vector<std::string> Positional;
        std::map<std::string, std::string> Values;
        std::set<std::string> Flags;

        bool Has(std::string const& name) const { return Flags.count(name) != 0 || Values.count(name) != 0; }

        std::string Get(std::string const& name, std::string const& fallback = {}) const
        {
            auto const it = Values.find(name);
            return it == Values.end() ? fallback : it->second;
        }

        uint32_t GetNumber(std::string const& name, uint32_t fallback) const
        {
            auto const it = Values.find(name);
            return it == Values.end() ? fallback : static_cast<uint32_t>(strtoul(it->second.c_str(), nullptr, 10));
        }
    };

    Options ParseOptions(int argc, wchar_t** argv, int first)
    {
        // options that take a value; everything else starting with -- is a flag
        static std::set<std::string> const valued = { "--seconds", "--type", "--name", "--port", "--sysex", "--timeout", "--filter", "--rtp-log", "--drop-every", "--dll" };

        Options options;

        for (int i = first; i < argc; i++)
        {
            auto const argument = ToUtf8(argv[i]);

            if (argument.rfind("--", 0) == 0)
            {
                if (valued.count(argument) && i + 1 < argc) options.Values[argument] = ToUtf8(argv[++i]);
                else options.Flags.insert(argument);
            }
            else
            {
                options.Positional.push_back(argument);
            }
        }

        return options;
    }

    Spike::SessionClock g_clock;

    double SinceStart(uint64_t sessionTicks)
    {
        auto const origin = g_clock.OriginTicks();
        return sessionTicks >= origin ? static_cast<double>(sessionTicks - origin) / 10.0 : -static_cast<double>(origin - sessionTicks) / 10.0;
    }

    std::string ComputerName()
    {
        wchar_t name[256]{};
        DWORD size = ARRAYSIZE(name);
        GetComputerNameExW(ComputerNameDnsHostname, name, &size);
        return ToUtf8(name);
    }

    void PrintParticipant(Participant const& p)
    {
        Print("  [%u] \"%s\" %s, %s", p.Id, p.RemoteName.c_str(), ParticipantStateName(p.State), p.WeInitiated ? "we invited" : "they invited");
        Print("      control %s  data %s  ssrc %08X", p.RemoteControl.ToString().c_str(), p.RemoteData.ToString().c_str(), p.RemoteSsrc);

        if (p.HaveClockOffset)
        {
            Print("      clock offset %+.1f ms (remote minus local), from the sample with the best round trip, %.1f ms",
                static_cast<double>(p.ClockOffset) / 10.0, static_cast<double>(p.Stats.BestRoundTripTicks) / 10.0);
            Print("      latest round trip %.1f ms, offset spread across kept samples %.1f ms, %u exchanges (%u started by the peer)",
                static_cast<double>(p.Stats.RoundTripTicks) / 10.0, static_cast<double>(p.Stats.ClockOffsetSpreadTicks) / 10.0,
                p.Stats.SyncExchanges, p.Stats.SyncExchangesStartedByPeer);
        }

        auto const& s = p.Stats;
        Print("      received %llu packets (%llu lost, %llu late), %llu messages, %llu with marker bit, %llu journals (%llu malformed)",
            s.PacketsReceived, s.PacketsLost, s.PacketsOutOfOrder, s.MessagesReceived, s.MarkerBitPackets, s.JournalsSeen, s.JournalsMalformed);
        Print("      loss events %llu (%llu covered by a journal), recovered note offs %llu, malformed packets %llu, malformed commands %llu",
            s.LossEvents, s.LossEventsCovered, s.RecoveredNoteOffs, s.MalformedPackets, s.MalformedCommands);
        Print("      sent %llu packets; feedback (RS) received %llu, last field %08X (low %u, high %u); journal trims %llu",
            s.PacketsSent, s.FeedbackReceived, s.LastFeedbackField, s.LastFeedbackField & 0xFFFF, s.LastFeedbackField >> 16, p.FeedbackTrims);

        if (s.BitrateLimit != 0) Print("      bit rate limit requested: %u bits/s", s.BitrateLimit);
        if (p.State == ParticipantState::Ended) Print("      ended: %s", EndReasonName(p.Reason));
    }

    std::string DescribeJournal(uint8_t const* data, size_t size)
    {
        RecoveryJournal journal{};
        if (!ParseRecoveryJournal(data, size, journal)) return "malformed (" + std::to_string(size) + " bytes)";

        std::string text = "checkpoint " + std::to_string(journal.CheckpointSequence) + (journal.SinglePacketLoss ? " S" : " S=0");

        if (journal.HasSystemJournal)
        {
            char part[48]{};
            snprintf(part, sizeof(part), ", system chapters %02X", journal.SystemChapters);
            text += part;
        }

        for (auto const& channel : journal.Channels)
        {
            char part[160]{};
            snprintf(part, sizeof(part), "; ch%u toc %02X on %zu off %zu%s%s", channel.Channel + 1, channel.Toc,
                channel.NoteOns.size(), channel.NoteOffs.size(),
                channel.HasPitchWheel ? " wheel" : "", channel.Controllers.empty() ? "" : " cc");
            text += part;
        }

        return text + " (" + std::to_string(size) + " bytes)";
    }

    // Wires one Session to a real port pair, receive threads and a tick thread.
    class LiveSession : public ISessionHost
    {
    public:
        LiveSession(SessionConfig config, Spike::PortPair& ports, bool verbose) :
            m_ports(ports), m_verbose(verbose), m_session(std::move(config), *this, Spike::SecureRandom64())
        {
        }

        ~LiveSession() { Stop(); }

        void Start()
        {
            m_ports.Control().StartReceiving([this](PeerAddress const& from, uint8_t const* data, size_t size)
            {
                auto lock = std::scoped_lock{ m_lock };
                m_controlDatagrams++;
                if (m_verbose) TraceIn(true, from, data, size);
                m_session.OnDatagram(true, from, data, size, g_clock.Now());
            });

            m_ports.Data().StartReceiving([this](PeerAddress const& from, uint8_t const* data, size_t size)
            {
                auto lock = std::scoped_lock{ m_lock };
                m_dataDatagrams++;

                AppleMidiCommand command{};
                bool const isRtp = !TryGetAppleMidiCommand(data, size, command);
                if (isRtp) m_rtpArrivals++;

                if (m_dropEvery != 0 && isRtp && m_rtpArrivals % m_dropEvery == 0)
                {
                    m_rtpDropped++;
                    if (size >= 4) Print("%10.1f ms  (deliberately dropped incoming RTP sequence %u)", SinceStart(g_clock.Now()), (data[2] << 8) | data[3]);
                    return;
                }

                if (m_verbose) TraceIn(false, from, data, size);
                m_session.OnDatagram(false, from, data, size, g_clock.Now());
            });

            m_ticker = std::thread([this]()
            {
                while (!m_stopping)
                {
                    {
                        auto lock = std::scoped_lock{ m_lock };
                        m_session.Tick(g_clock.Now());
                    }

                    Sleep(5);
                }
            });
        }

        void Stop()
        {
            if (m_stopped.exchange(true)) return;

            {
                auto lock = std::scoped_lock{ m_lock };
                m_session.EndAll(g_clock.Now());
                m_session.Tick(g_clock.Now());
            }

            m_stopping = true;
            if (m_ticker.joinable()) m_ticker.join();

            m_ports.Close();
        }

        template <typename Function>
        auto With(Function&& function)
        {
            auto lock = std::scoped_lock{ m_lock };
            return function(m_session);
        }

        void DropNextDataPacket() { m_dropNextDataPacket = true; }
        void SetRtpLog(uint32_t packets) { m_rtpReportLimit = packets; }
        void SetDropEvery(uint32_t every) { m_dropEvery = every; }
        uint64_t RtpDropped() const { return m_rtpDropped; }

        void PrintCounters()
        {
            auto lock = std::scoped_lock{ m_lock };
            auto const& stats = m_session.Stats();

            Print("      sockets: %llu control datagrams, %llu data datagrams (%llu RTP, %llu dropped on purpose); session ignored %llu unknown, %llu RTP with no match",
                m_controlDatagrams, m_dataDatagrams, m_rtpArrivals, m_rtpDropped, stats.UnknownDatagrams, stats.UnexpectedRtp);
        }

        // ISessionHost

        void SendControl(PeerAddress const& to, std::vector<uint8_t> const& datagram) override
        {
            if (m_verbose) TraceOut(true, to, datagram);
            m_ports.Control().Send(to, datagram);
        }

        void SendData(PeerAddress const& to, std::vector<uint8_t> const& datagram) override
        {
            AppleMidiCommand command{};
            if (m_dropNextDataPacket && !TryGetAppleMidiCommand(datagram.data(), datagram.size(), command))
            {
                m_dropNextDataPacket = false;
                Print("  (simulated loss: RTP sequence %u not sent)", (datagram[2] << 8) | datagram[3]);
                return;
            }

            if (m_verbose) TraceOut(false, to, datagram);
            m_ports.Data().Send(to, datagram);
        }

        void OnMidi(Participant const& participant, uint64_t localTimestamp, int64_t lead, bool recovered, std::vector<uint8_t> const& bytes) override
        {
            m_received.insert(m_received.end(), bytes.begin(), bytes.end());
            m_leads.push_back(lead);

            if (!m_printMidi) return;

            Print("%10.1f ms  <- \"%s\"  %-24s  %-30s  sender time %+.1f ms%s",
                SinceStart(localTimestamp), participant.RemoteName.c_str(), Spike::Hex(bytes, 12).c_str(),
                Spike::DescribeMidi(bytes).c_str(), static_cast<double>(lead) / 10.0, recovered ? "  RECOVERED" : "");
        }

        void OnParticipantChanged(Participant const& participant) override
        {
            Print("%10.1f ms  participant [%u] \"%s\" at %s: %s%s%s", SinceStart(g_clock.Now()), participant.Id,
                participant.RemoteName.c_str(), participant.RemoteControl.ToString().c_str(), ParticipantStateName(participant.State),
                participant.State == ParticipantState::Ended ? ", " : "",
                participant.State == ParticipantState::Ended ? EndReasonName(participant.Reason) : "");

            if (participant.State == ParticipantState::Ended) m_ended.push_back(participant);
        }

        void Log(std::string const& message) override
        {
            Print("%10.1f ms  %s", SinceStart(g_clock.Now()), message.c_str());
        }

        void OnRtpPacket(Participant const& participant, DecodedPacket const& packet, uint8_t const* datagram, size_t size) override
        {
            // the first packets say what this peer's sender does on the wire
            if (m_rtpReports++ >= m_rtpReportLimit && !m_verbose) return;

            std::string journal = "no journal";
            if (packet.HasJournal) journal = "journal: " + DescribeJournal(datagram + packet.JournalOffset, packet.JournalSize);

            Print("%10.1f ms  rtp from \"%s\": seq %u, ts %u, M=%d, %zu events; %s", SinceStart(g_clock.Now()), participant.RemoteName.c_str(),
                packet.Header.SequenceNumber, packet.Header.Timestamp, packet.Header.Marker ? 1 : 0, packet.Events.size(), journal.c_str());
            Print("              bytes %s", Spike::Hex(datagram, size, 72).c_str());
        }

        std::vector<uint8_t> const& ReceivedStream() const { return m_received; }
        std::vector<int64_t> const& Leads() const { return m_leads; }
        std::vector<Participant> const& Ended() const { return m_ended; }
        void SetPrintMidi(bool print) { m_printMidi = print; }

    private:
        void TraceIn(bool control, PeerAddress const& from, uint8_t const* data, size_t size)
        {
            AppleMidiCommand command{};
            char const* what = TryGetAppleMidiCommand(data, size, command) ? AppleMidiCommandName(command) : "RTP";
            Print("%10.1f ms  %s in  %-3s from %s: %s", SinceStart(g_clock.Now()), control ? "control" : "data   ", what, from.ToString().c_str(), Spike::Hex(data, size, 40).c_str());
        }

        void TraceOut(bool control, PeerAddress const& to, std::vector<uint8_t> const& datagram)
        {
            AppleMidiCommand command{};
            char const* what = TryGetAppleMidiCommand(datagram.data(), datagram.size(), command) ? AppleMidiCommandName(command) : "RTP";
            Print("%10.1f ms  %s out %-3s to   %s: %s", SinceStart(g_clock.Now()), control ? "control" : "data   ", what, to.ToString().c_str(), Spike::Hex(datagram, 40).c_str());
        }

        Spike::PortPair& m_ports;
        bool m_verbose{ false };
        std::mutex m_lock;
        Session m_session;
        std::thread m_ticker;
        std::atomic<bool> m_stopping{ false };
        std::atomic<bool> m_stopped{ false };
        std::atomic<bool> m_dropNextDataPacket{ false };
        std::vector<uint8_t> m_received;
        std::vector<int64_t> m_leads;
        std::vector<Participant> m_ended;
        uint32_t m_rtpReports{ 0 };
        uint32_t m_rtpReportLimit{ 6 };
        uint32_t m_dropEvery{ 0 };
        uint64_t m_rtpArrivals{ 0 };
        uint64_t m_rtpDropped{ 0 };
        uint64_t m_controlDatagrams{ 0 };
        uint64_t m_dataDatagrams{ 0 };
        bool m_printMidi{ true };
    };

    bool WaitFor(std::function<bool()> const& condition, uint32_t milliseconds)
    {
        auto const end = GetTickCount64() + milliseconds;

        while (!Spike::StopRequested() && GetTickCount64() < end)
        {
            if (condition()) return true;
            Sleep(20);
        }

        return condition();
    }

    void RunFor(uint32_t seconds, std::function<void()> const& everyFiveSeconds)
    {
        auto const end = GetTickCount64() + static_cast<uint64_t>(seconds) * 1000;
        auto next = GetTickCount64() + 5000;

        while (!Spike::StopRequested() && GetTickCount64() < end)
        {
            Sleep(50);

            if (GetTickCount64() >= next)
            {
                next += 5000;
                if (everyFiveSeconds) everyFiveSeconds();
            }
        }
    }

    // Quiet by default: a Note On at velocity 1 on channel 16 with its Note Off, an undefined
    // controller, and a Universal Identity Request, so a Mac app listening hears nothing.
    std::vector<uint8_t> TestPattern(bool audible)
    {
        if (audible)
        {
            return { 0x90, 60, 90, 0x80, 60, 64 };
        }

        return { 0x9F, 0x00, 0x01, 0x8F, 0x00, 0x40, 0xBF, 0x03, 0x00, 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
    }

    std::vector<uint8_t> MakeSysEx(uint32_t totalBytes)
    {
        totalBytes = (std::max)(totalBytes, 4u);

        // manufacturer 7D is reserved for non-commercial and educational use
        std::vector<uint8_t> sysex(totalBytes);
        sysex.front() = 0xF0;
        sysex[1] = 0x7D;
        for (size_t i = 2; i + 1 < sysex.size(); i++) sysex[i] = static_cast<uint8_t>(i % 0x7F);
        sysex.back() = 0xF7;
        return sysex;
    }

    // ------------------------------------------------------------------------------------------

    int CommandBrowse(Options const& options)
    {
        auto const type = ToWide(options.Get("--type", "_apple-midi._udp.local"));
        auto const seconds = options.GetNumber("--seconds", 8);

        Print("Browsing %s for %u seconds with DnsServiceBrowse (the Windows mDNS stack)...", ToUtf8(type).c_str(), seconds);

        WindowsMidiServicesInternal::MidiDnssdBrowser browser;

        auto const status = browser.Start(
            type,
            [](WindowsMidiServicesInternal::MidiDnssdService const& service) { Spike::PrintService(service, "added  "); },
            [](WindowsMidiServicesInternal::MidiDnssdService const& service, uint32_t) { Spike::PrintService(service, "updated"); },
            [](std::wstring const& fullName, std::wstring const&) { Print("  removed \"%s\"", ToUtf8(fullName).c_str()); });

        if (FAILED(status))
        {
            Print("DnsServiceBrowse failed to start: 0x%08X", static_cast<unsigned>(status));
            return 1;
        }

        RunFor(seconds, nullptr);
        browser.Stop();

        Print("%zu service(s) resolved.", browser.EnumeratedServices().size());
        return 0;
    }

    int CommandRegister(Options const& options)
    {
        auto const label = options.Get("--name", "RTP-MIDI prototype on " + ComputerName());
        auto const port = static_cast<uint16_t>(options.GetNumber("--port", 5004));
        auto const seconds = options.GetNumber("--seconds", 20);
        auto const type = ToWide(options.Get("--type", "_apple-midi._udp.local"));

        std::vector<std::pair<std::wstring, std::wstring>> text;
        if (options.Has("--txt")) text = { { L"UMPEndpointName", L"probe" }, { L"ProductInstanceId", L"probe" } };

        Spike::DnssdAdvertiser advertiser;

        auto const started = GetTickCount64();
        auto const registered = advertiser.Register(ToWide(label), port, 10000, type, text);
        auto const elapsed = GetTickCount64() - started;

        Print("DnsServiceRegister \"%s\" on %s port %u: %s (status %lu) after %llu ms",
            ToUtf8(advertiser.RequestedName()).c_str(), ToUtf8(advertiser.HostName()).c_str(), port,
            registered ? "registered" : "FAILED", advertiser.Status(), elapsed);

        if (!advertiser.RegisteredName().empty())
        {
            Print("  registered as \"%s\"%s", ToUtf8(advertiser.RegisteredName()).c_str(),
                _wcsicmp(advertiser.RegisteredName().c_str(), advertiser.RequestedName().c_str()) != 0 ? "  (RENAMED by the responder)" : "");
        }

        if (!registered) return 1;

        if (options.Has("--follow-up"))
        {
            auto registeredLabel = advertiser.RegisteredName().empty() ? advertiser.RequestedName() : advertiser.RegisteredName();
            auto const suffix = L"." + type;

            if (registeredLabel.size() > suffix.size() && _wcsicmp(registeredLabel.c_str() + registeredLabel.size() - suffix.size(), suffix.c_str()) == 0)
            {
                registeredLabel.resize(registeredLabel.size() - suffix.size());
            }

            auto const packets = RtpMidiMdns::BuildPtrAnnouncements(ToUtf8(type), { ToUtf8(registeredLabel) }, 4500, 1200);
            auto const registeredAt = started + elapsed;

            for (auto const delay : { 1500ull, 4500ull })
            {
                while (GetTickCount64() < registeredAt + delay) Sleep(10);

                auto const result = RtpMidiMdns::SendAnnouncements(packets);

                Print("  follow-up announcement at +%llu ms: %zu packet(s) on %u IPv4 and %u IPv6 interfaces, last error %d",
                    delay, packets.size(), result.IPv4Interfaces, result.IPv6Interfaces, result.LastError);
            }
        }

        if (options.Has("--terminate"))
        {
            // the documented promise is that the registration dies with the process
            Print("Terminating the process without deregistering, %u seconds from now.", seconds);
            RunFor(seconds, nullptr);
            TerminateProcess(GetCurrentProcess(), 3);
        }

        Print("Holding the registration for %u seconds.", seconds);
        RunFor(seconds, nullptr);

        advertiser.Unregister();
        Print("Deregistered.");
        return 0;
    }

    int CommandListen(Options const& options)
    {
        auto const label = options.Get("--name", "RTP-MIDI prototype on " + ComputerName());
        auto const seconds = options.GetNumber("--seconds", 120);

        Spike::PortPair ports;
        std::string failure;

        auto const preferred = static_cast<uint16_t>(options.GetNumber("--port", 0));
        bool bound = preferred != 0 ? ports.Bind(preferred, 0, 0, failure) : ports.Bind(0, 5004, 5100, failure);

        if (!bound)
        {
            Print("Could not bind a port pair: %s", failure.c_str());
            return 1;
        }

        SessionConfig config{};
        config.LocalName = label;
        config.Ssrc = static_cast<uint32_t>(Spike::SecureRandom64());
        config.AcceptInvitations = true;
        config.SendJournal = options.Has("--journal");
        config.FeedbackSequenceInHighBits = !options.Has("--feedback-low");

        LiveSession live(config, ports, options.Has("--verbose"));
        live.SetRtpLog(options.GetNumber("--rtp-log", 6));
        live.SetDropEvery(options.GetNumber("--drop-every", 0));
        live.Start();

        Print("Session \"%s\" listening on control %u, data %u.", label.c_str(), ports.Control().Port(), ports.Data().Port());

        Spike::DnssdAdvertiser advertiser;

        if (!options.Has("--no-advertise"))
        {
            auto const registered = advertiser.Register(ToWide(label), ports.Control().Port(), 10000);
            Print("Advertised as _apple-midi._udp on %s: %s (status %lu)%s", ToUtf8(advertiser.HostName()).c_str(),
                registered ? "ok" : "FAILED", advertiser.Status(),
                (!advertiser.RegisteredName().empty() && _wcsicmp(advertiser.RegisteredName().c_str(), advertiser.RequestedName().c_str()) != 0) ?
                ("  renamed to " + ToUtf8(advertiser.RegisteredName())).c_str() : "");
        }

        Print("Waiting %u seconds for invitations. Ctrl+C to stop.", seconds);

        bool const sendTest = options.Has("--send-test");
        bool const echo = options.Has("--echo");
        size_t echoed = 0;

        RunFor(seconds, [&]()
        {
            auto const participants = live.With([](Session& session) { return session.Snapshot(); });
            Print("%10.1f ms  %zu participant(s)", SinceStart(g_clock.Now()), participants.size());
            live.PrintCounters();
            for (auto const& participant : participants) PrintParticipant(participant);

            if (sendTest)
            {
                auto const pattern = TestPattern(options.Has("--notes"));
                live.With([&](Session& session) { session.SendMidi(pattern.data(), pattern.size(), g_clock.Now()); return 0; });
            }

            if (echo)
            {
                auto const& stream = live.ReceivedStream();
                std::vector<uint8_t> fresh(stream.begin() + static_cast<std::ptrdiff_t>(echoed), stream.end());
                echoed = stream.size();
                if (!fresh.empty()) live.With([&](Session& session) { session.SendMidi(fresh.data(), fresh.size(), g_clock.Now()); return 0; });
            }
        });

        auto const final = live.With([](Session& session) { return session.Snapshot(); });
        Print("Final state (%llu incoming RTP packets deliberately dropped):", live.RtpDropped());
        for (auto const& participant : final) PrintParticipant(participant);
        for (auto const& participant : live.Ended()) PrintParticipant(participant);

        live.Stop();
        advertiser.Unregister();

        return 0;
    }

    bool ResolveTarget(std::string const& target, uint32_t timeoutSeconds, PeerAddress& remote)
    {
        // address:port, [v6]:port, or else an instance label to find with DNS-SD
        auto const colon = target.rfind(':');

        if (colon != std::string::npos && colon + 1 < target.size())
        {
            auto host = target.substr(0, colon);
            auto const port = static_cast<uint16_t>(strtoul(target.c_str() + colon + 1, nullptr, 10));

            if (host.size() > 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);

            if (port != 0 && Spike::TryParseAddress(ToWide(host), port, remote)) return true;
        }

        Print("Looking for \"%s\" with DnsServiceBrowse (up to %u s)...", target.c_str(), timeoutSeconds);

        WindowsMidiServicesInternal::MidiDnssdService service{};

        auto const started = GetTickCount64();
        if (!Spike::FindAppleMidiService(ToWide(target), timeoutSeconds * 1000, service))
        {
            Print("Not found.");
            return false;
        }

        Print("Resolved after %llu ms:", GetTickCount64() - started);
        Spike::PrintService(service, "found  ");

        std::string why;
        if (!Spike::ChooseServiceAddress(service, remote, why))
        {
            Print("No usable address: %s", why.c_str());
            return false;
        }

        Print("Using %s (%s).", remote.ToString().c_str(), why.c_str());
        return true;
    }

    int CommandConnect(Options const& options)
    {
        if (options.Positional.empty())
        {
            Print("connect needs a target: an instance label or address:port");
            return 1;
        }

        auto const seconds = options.GetNumber("--seconds", 30);

        PeerAddress remote{};
        if (!ResolveTarget(options.Positional[0], options.GetNumber("--timeout", 10), remote)) return 1;

        Spike::PortPair ports;
        std::string failure;

        auto const preferred = static_cast<uint16_t>(options.GetNumber("--port", 0));
        bool const bound = preferred != 0 ? ports.Bind(preferred, 0, 0, failure) : ports.Bind(0, 5102, 5300, failure);

        if (!bound)
        {
            Print("Could not bind a port pair: %s", failure.c_str());
            return 1;
        }

        SessionConfig config{};
        config.LocalName = options.Get("--name", ComputerName() + " (RTP-MIDI prototype)");
        config.Ssrc = static_cast<uint32_t>(Spike::SecureRandom64());
        config.AcceptInvitations = false;
        config.SendJournal = options.Has("--journal");
        config.FeedbackSequenceInHighBits = !options.Has("--feedback-low");

        LiveSession live(config, ports, options.Has("--verbose"));
        live.SetRtpLog(options.GetNumber("--rtp-log", 6));
        live.SetDropEvery(options.GetNumber("--drop-every", 0));
        live.Start();

        Print("Local session \"%s\" on control %u, data %u. Inviting %s...", config.LocalName.c_str(), ports.Control().Port(), ports.Data().Port(), remote.ToString().c_str());

        auto const invitedAt = g_clock.Now();
        live.With([&](Session& session) { return session.Invite(remote, g_clock.Now()); });

        auto const connected = WaitFor([&]() { return live.With([](Session& session) { return session.ConnectedCount() > 0; }); }, 15000);

        if (!connected)
        {
            Print("Not connected after 15 s.");
            for (auto const& participant : live.Ended()) PrintParticipant(participant);
            for (auto const& participant : live.With([](Session& session) { return session.Snapshot(); })) PrintParticipant(participant);
            live.Stop();
            return 2;
        }

        Print("Connected %.1f ms after the first invitation.", static_cast<double>(g_clock.Now() - invitedAt) / 10.0);

        if (options.Has("--send-test") || options.Has("--notes"))
        {
            bool const audible = options.Has("--notes");

            if (audible)
            {
                // a slow arpeggio a person can hear and a monitor can check
                static constexpr uint8_t arpeggio[] = { 60, 64, 67, 72 };

                for (auto const note : arpeggio)
                {
                    uint8_t const on[] = { 0x90, note, 90 };
                    live.With([&](Session& session) { session.SendMidi(on, sizeof(on), g_clock.Now()); return 0; });
                    Sleep(250);

                    if (options.Has("--simulate-loss") && note == 64) live.DropNextDataPacket();

                    uint8_t const off[] = { 0x80, note, 64 };
                    live.With([&](Session& session) { session.SendMidi(off, sizeof(off), g_clock.Now()); return 0; });
                    Sleep(100);
                }

                Print("Sent a C major arpeggio (60 64 67 72) on channel 1%s.", options.Has("--simulate-loss") ? ", with the Note Off for 64 deliberately not sent" : "");
            }
            else
            {
                auto const pattern = TestPattern(false);
                live.With([&](Session& session) { session.SendMidi(pattern.data(), pattern.size(), g_clock.Now()); return 0; });
                Print("Sent the quiet test pattern: %s", Spike::Hex(pattern).c_str());
            }
        }

        if (auto const sysexBytes = options.GetNumber("--sysex", 0); sysexBytes > 0)
        {
            auto const sysex = MakeSysEx(sysexBytes);
            live.With([&](Session& session) { session.SendMidi(sysex.data(), sysex.size(), g_clock.Now()); return 0; });
            Print("Sent a %u byte SysEx (manufacturer 7D).", sysexBytes);
        }

        Print("Staying connected for %u seconds. Ctrl+C to stop.", seconds);

        RunFor(seconds, [&]()
        {
            for (auto const& participant : live.With([](Session& session) { return session.Snapshot(); })) PrintParticipant(participant);
        });

        Print("Final state:");
        for (auto const& participant : live.With([](Session& session) { return session.Snapshot(); })) PrintParticipant(participant);
        for (auto const& participant : live.Ended()) PrintParticipant(participant);

        live.Stop();
        return 0;
    }

    // Both roles in one process over real sockets bound to ::1 only: listener, initiator, MIDI
    // both ways including a large SysEx, then BY.
    int CommandLoopback(Options const& options)
    {
        auto const sysexBytes = options.GetNumber("--sysex", 20000);

        Spike::PortPair listenerPorts;
        Spike::PortPair initiatorPorts;
        std::string failure;

        if (!listenerPorts.Bind(0, 5302, 5400, failure, true) || !initiatorPorts.Bind(0, 5402, 5500, failure, true))
        {
            Print("Could not bind port pairs: %s", failure.c_str());
            return 1;
        }

        SessionConfig listenerConfig{};
        listenerConfig.LocalName = "Loopback listener";
        listenerConfig.Ssrc = static_cast<uint32_t>(Spike::SecureRandom64());
        listenerConfig.SendJournal = true;

        SessionConfig initiatorConfig{};
        initiatorConfig.LocalName = "Loopback initiator";
        initiatorConfig.Ssrc = static_cast<uint32_t>(Spike::SecureRandom64());
        initiatorConfig.AcceptInvitations = false;
        initiatorConfig.SendJournal = true;

        LiveSession listener(listenerConfig, listenerPorts, false);
        LiveSession initiator(initiatorConfig, initiatorPorts, false);
        listener.SetPrintMidi(false);
        initiator.SetPrintMidi(false);
        listener.Start();
        initiator.Start();

        PeerAddress target{};
        Spike::TryParseAddress(L"::1", listenerPorts.Control().Port(), target);

        auto const invitedAt = g_clock.Now();
        initiator.With([&](Session& session) { return session.Invite(target, g_clock.Now()); });

        bool const connected = WaitFor([&]()
        {
            return initiator.With([](Session& s) { return s.ConnectedCount() == 1; }) &&
                listener.With([](Session& s) { return s.ConnectedCount() == 1; });
        }, 5000);

        Print("Loopback over ::1: %s after %.1f ms (listener %u/%u, initiator %u/%u)", connected ? "connected" : "NOT connected",
            static_cast<double>(g_clock.Now() - invitedAt) / 10.0, listenerPorts.Control().Port(), listenerPorts.Data().Port(),
            initiatorPorts.Control().Port(), initiatorPorts.Data().Port());

        if (!connected) return 2;

        std::vector<uint8_t> toListener;
        for (int i = 0; i < 200; i++) toListener.insert(toListener.end(), { 0x90, static_cast<uint8_t>(i % 128), 0x40, 0x80, static_cast<uint8_t>(i % 128), 0x40 });
        auto const sysex = MakeSysEx(sysexBytes);
        toListener.insert(toListener.end(), sysex.begin(), sysex.end());

        std::vector<uint8_t> const toInitiator = { 0xB0, 0x07, 0x64, 0xE0, 0x00, 0x40, 0xF8, 0xFA, 0xFC };

        // sent in pieces, the way a translator hands over a byte stream
        for (size_t offset = 0; offset < toListener.size(); offset += 97)
        {
            auto const count = (std::min)(static_cast<size_t>(97), toListener.size() - offset);
            initiator.With([&](Session& s) { s.SendMidi(toListener.data() + offset, count, g_clock.Now()); return 0; });
        }

        listener.With([&](Session& s) { s.SendMidi(toInitiator.data(), toInitiator.size(), g_clock.Now()); return 0; });

        WaitFor([&]() { return listener.ReceivedStream().size() >= toListener.size() && initiator.ReceivedStream().size() >= toInitiator.size(); }, 5000);

        bool const listenerOk = listener.ReceivedStream() == toListener;
        bool const initiatorOk = initiator.ReceivedStream() == toInitiator;

        auto const leads = listener.Leads();
        int64_t worst = 0;
        for (auto const lead : leads) worst = (std::min)(worst, lead);

        Print("  initiator -> listener: %zu bytes incl. a %u byte SysEx: %s", toListener.size(), sysexBytes, listenerOk ? "identical" : "MISMATCH");
        Print("  listener -> initiator: %zu bytes: %s", toInitiator.size(), initiatorOk ? "identical" : "MISMATCH");
        Print("  worst mapped transit %.1f ms over %zu messages", static_cast<double>(-worst) / 10.0, leads.size());

        // let a round of clock sync and feedback happen
        Sleep(1500);

        for (auto const& participant : initiator.With([](Session& s) { return s.Snapshot(); })) PrintParticipant(participant);
        for (auto const& participant : listener.With([](Session& s) { return s.Snapshot(); })) PrintParticipant(participant);

        initiator.Stop();

        bool const ended = WaitFor([&]() { return listener.With([](Session& s) { return s.Snapshot().empty(); }); }, 2000);
        Print("  BY from the initiator %s the listener's participant", ended ? "removed" : "did NOT remove");

        listener.Stop();

        return (listenerOk && initiatorOk && ended) ? 0 : 3;
    }
}

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCtrlHandler(Spike::ConsoleControlHandler, TRUE);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        Print("WSAStartup failed");
        return 1;
    }

    auto const command = argc > 1 ? ToUtf8(argv[1]) : std::string{ "help" };
    auto const options = ParseOptions(argc, argv, 2);

    int result = 0;

    if (command == "selftest") result = RunSelfTest();
    else if (command == "browse") result = CommandBrowse(options);
    else if (command == "register") result = CommandRegister(options);
    else if (command == "listen") result = CommandListen(options);
    else if (command == "connect") result = CommandConnect(options);
    else if (command == "loopback") result = CommandLoopback(options);
    else if (command == "mdns-watch") result = Spike::WatchMdns(options.Get("--filter", "_apple-midi"), options.GetNumber("--seconds", 20), options.Has("--queries"));
#ifdef RTP_TRANSPORT_TEST
    else if (command == "transport-test")
    {
        // the transport builds into the same output folder as this executable
        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, ARRAYSIZE(exePath));

        std::wstring defaultDll{ exePath };
        defaultDll = defaultDll.substr(0, defaultDll.find_last_of(L'\\') + 1) + L"Midi2.RtpMidiTransport.dll";

        result = RunTransportTest(options.Has("--dll") ? ToWide(options.Get("--dll")) : defaultDll);
    }
#endif
#ifdef RTP_SDK_CHECK
    else if (command == "service") result = RunServiceCommand(std::vector<std::wstring>(argv + 2, argv + argc));
#endif
    else if (command == "register-winrt")
    {
        auto const fullName = ToWide(options.Get("--name", "WinRT probe") + "." + options.Get("--type", "_wmsprobe._udp.local"));
        result = RegisterWithWinRt(fullName, static_cast<uint16_t>(options.GetNumber("--port", 5030)), options.GetNumber("--seconds", 5), options.Has("--txt"));
    }
    else
    {
        Print("rtpmidi-spike selftest | browse | register | listen | connect <target> | loopback | mdns-watch | transport-test | service");
        Print("See the comment at the top of main.cpp for the options.");
    }

    WSACleanup();
    return result;
}
