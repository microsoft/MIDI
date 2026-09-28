// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. Drives the RTP-MIDI transport in the running MIDI service through the RTP-MIDI SDK,
// until a settings app has a page for it.
//
//   rtpmidi-spike service status
//   rtpmidi-spike service host <name> [--port P] [--service-name LABEL] [--no-advertise] [--no-journal]
//   rtpmidi-spike service connect <advertised name | address:port> [--name NAME] [--endpoint-name NAME]
//                                 [--no-reconnect] [--no-journal]
//   rtpmidi-spike service remove <entry id, its first few characters, or all>
//   rtpmidi-spike service loop-test <send endpoint> [--listen ENDPOINT,ENDPOINT] [--group 1-16] [--notes N]
//                                   [--interval MS] [--sysex BYTES,BYTES | none] [--pace-us US]
//
// Entries last until the service restarts. Nothing is written to the configuration file.
// loop-test is for a device whose output is cabled or routed back to an input: it sends numbered
// notes and SysEx through one endpoint and reports what comes back on each listened endpoint.
// ============================================================================

#include "spike_common.h"

#undef GetObject
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Transports.Rtp.h>

#include <algorithm>
#include <cwctype>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace rtp = winrt::Windows::Devices::Midi2::Transports::Rtp;
namespace midi2 = winrt::Windows::Devices::Midi2;
namespace enumeration = winrt::Windows::Devices::Midi2::Enumeration;

using Spike::Print;
using Spike::ToUtf8;

bool SdkCheckStart(std::wstring const& dllPath);
std::wstring SdkLoadMidi2Runtime();

namespace
{
    struct Arguments
    {
        std::vector<std::wstring> Positional;
        std::map<std::wstring, std::wstring> Values;
        std::vector<std::wstring> Flags;

        bool Has(std::wstring const& flag) const
        {
            return std::find(Flags.begin(), Flags.end(), flag) != Flags.end() || Values.count(flag) != 0;
        }

        std::wstring Get(std::wstring const& name) const
        {
            auto const it = Values.find(name);
            return it == Values.end() ? std::wstring{} : it->second;
        }
    };

    Arguments Parse(std::vector<std::wstring> const& raw)
    {
        static std::vector<std::wstring> const valued{ L"--port", L"--service-name", L"--name", L"--endpoint-name",
            L"--listen", L"--group", L"--notes", L"--interval", L"--sysex", L"--pace-us" };

        Arguments arguments;

        for (size_t i = 0; i < raw.size(); i++)
        {
            auto const& argument = raw[i];

            if (argument.rfind(L"--", 0) == 0)
            {
                if (std::find(valued.begin(), valued.end(), argument) != valued.end() && i + 1 < raw.size()) arguments.Values[argument] = raw[++i];
                else arguments.Flags.push_back(argument);
            }
            else
            {
                arguments.Positional.push_back(argument);
            }
        }

        return arguments;
    }

    std::string Text(winrt::hstring const& value)
    {
        return ToUtf8(std::wstring{ value });
    }

    double TicksToMilliseconds(uint64_t const ticks)
    {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);

        return frequency.QuadPart == 0 ? 0.0 : static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency.QuadPart);
    }

    std::string DescribeError(int32_t const code)
    {
        switch (static_cast<uint32_t>(code))
        {
        case 0: return "none";
        case 0x800705B4: return "no answer";
        case 0x80070005: return "the remote refused the invitation";
        case 0x800704CA: return "the remote ended the connection";
        case 0x800704D4: return "clock sync stopped, so the remote is probably gone";
        case 0x800704D0: return "not advertised right now, or not reachable";
        case 0x800704CB: return "no free port";
        default: break;
        }

        wchar_t buffer[512]{};
        auto const length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, static_cast<DWORD>(code), 0, buffer, ARRAYSIZE(buffer), nullptr);

        std::wstring message{ buffer, length };
        while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) message.pop_back();

        char hex[16]{};
        snprintf(hex, sizeof(hex), "0x%08X", static_cast<uint32_t>(code));

        return message.empty() ? hex : ToUtf8(message) + " (" + hex + ")";
    }

    char const* StateName(rtp::MidiRtpClientEntryState const state)
    {
        switch (state)
        {
        case rtp::MidiRtpClientEntryState::Active: return "connected";
        case rtp::MidiRtpClientEntryState::Failed: return "not connected, retrying";
        case rtp::MidiRtpClientEntryState::Unavailable: return "stopped";
        default: return "connecting";
        }
    }

    std::wstring GuidText(winrt::guid const& id)
    {
        return std::wstring{ winrt::to_hstring(id) };
    }

    // Lowercase, and the typographic apostrophes and quotes macOS puts in names made plain, so a
    // name typed in a terminal can match "Pete’s MacBook Pro"
    std::wstring Fold(std::wstring const& text)
    {
        std::wstring folded;
        folded.reserve(text.size());

        for (auto ch : text)
        {
            if (ch == L'\u2018' || ch == L'\u2019' || ch == L'\u02BC' || ch == L'`' || ch == L'\u00B4') ch = L'\'';
            else if (ch == L'\u201C' || ch == L'\u201D') ch = L'"';

            folded.push_back(static_cast<wchar_t>(std::towlower(ch)));
        }

        return folded;
    }

    std::wstring IdKey(std::wstring text)
    {
        std::erase_if(text, [](wchar_t ch) { return ch == L'{' || ch == L'}'; });
        return Fold(text);
    }

    void PrintConnection(rtp::MidiRtpConnection const& connection)
    {
        auto const hostName = connection.RemoteHostName().empty() ? std::string{} : " (" + Text(connection.RemoteHostName()) + ")";

        Print("      [%u] \"%s\" at %s:%u%s, %s, %s", connection.ConnectionId(), Text(connection.RemoteName()).c_str(),
            Text(connection.RemoteAddress()).c_str(), connection.RemotePort(), hostName.c_str(),
            connection.IsConnected() ? "connected" : "connecting",
            connection.ThisPcInvited() ? "this PC invited" : "the remote invited");

        Print("          round trip %.1f ms (best %.1f ms); packets %llu sent, %llu received, %llu lost, %llu repaired from the journal",
            TicksToMilliseconds(connection.CurrentLatencyTicks()), TicksToMilliseconds(connection.BestLatencyTicks()),
            connection.TotalCountNetworkPacketsSent(), connection.TotalCountNetworkPacketsReceived(),
            connection.TotalCountPacketsLost(), connection.TotalCountLossesRepairedFromJournal());

        Print("          messages %llu sent, %llu received; stuck notes ended %llu",
            connection.TotalCountMessagesSent(), connection.TotalCountMessagesReceived(), connection.TotalCountNoteOffsRecovered());

        if (!connection.EndpointDeviceId().empty()) Print("          endpoint %s", Text(connection.EndpointDeviceId()).c_str());
    }

    void PrintHost(rtp::MidiRtpConfiguredHost const& host)
    {
        Print("  %s  \"%s\"", ToUtf8(GuidText(host.HostId())).c_str(), Text(host.Name()).c_str());

        if (host.HasStarted())
        {
            Print("      started on port %s%s%s", Text(host.ActualPort()).c_str(),
                host.UsedPortFallback() ? " (the configured port was taken)" : "",
                host.IsEnabled() ? "" : ", stopped");

            if (host.Advertise())
            {
                Print("      advertised as \"%s\"%s", Text(host.ActualServiceInstanceName()).c_str(),
                    host.ServiceInstanceNameWasChanged() ? " (renamed by the DNS client, because another device uses the name)" : "");
            }
            else
            {
                Print("      not advertised");
            }
        }
        else
        {
            Print("      not started%s", host.IsEnabled() ? "" : ": stopped");
        }

        if (host.LastErrorCode() != 0) Print("      last error: %s", DescribeError(host.LastErrorCode()).c_str());

        auto const connections = host.Connections();
        if (connections.Size() == 0) Print("      no connections");
        for (auto const& connection : connections) PrintConnection(connection);
    }

    void PrintClient(rtp::MidiRtpConfiguredClient const& client)
    {
        auto const target = client.IsDirectConnection() ?
            Text(client.ConfiguredDirectAddress()) + ":" + std::to_string(client.ConfiguredDirectPort()) :
            "\"" + Text(client.RemoteServiceInstanceName()) + "\"";

        Print("  %s  \"%s\" to %s: %s", ToUtf8(GuidText(client.ClientId())).c_str(), Text(client.Name()).c_str(), target.c_str(), StateName(client.EntryState()));

        if (client.LastErrorCode() != 0) Print("      last error: %s", DescribeError(client.LastErrorCode()).c_str());
        if (!client.CustomEndpointName().empty()) Print("      endpoint name \"%s\"", Text(client.CustomEndpointName()).c_str());
        if (client.Connection() != nullptr) PrintConnection(client.Connection());
    }

    int CommandStatus()
    {
        auto const hosts = rtp::MidiRtpTransportManager::GetConfiguredHosts();
        auto const clients = rtp::MidiRtpTransportManager::GetConfiguredClients();
        auto const peers = rtp::MidiRtpTransportManager::GetAdvertisedPeers();

        Print("Hosts on this PC (%u)", hosts.Size());
        for (auto const& host : hosts) PrintHost(host);

        Print("");
        Print("Connections from this PC (%u)", clients.Size());
        for (auto const& client : clients) PrintClient(client);

        Print("");
        Print("RTP-MIDI devices advertised on the network (%u)", peers.Size());

        for (auto const& peer : peers)
        {
            std::string addresses;
            for (auto const& address : peer.IPv4Addresses()) addresses += (addresses.empty() ? "" : ", ") + Text(address);

            Print("  \"%s\"  %s:%u  %s%s", Text(peer.ServiceInstanceName()).c_str(), Text(peer.HostName()).c_str(), peer.Port(),
                addresses.c_str(), peer.IsThisPc() ? "  (this PC)" : "");
        }

        return 0;
    }

    bool TryParsePort(std::wstring const& text, uint16_t& port)
    {
        port = 0;

        if (text.empty() || text.size() > 5 || !std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; })) return false;

        auto const value = std::stoul(text);
        if (value < 1024 || value > 65534) return false;

        port = static_cast<uint16_t>(value);
        return true;
    }

    int CommandHost(Arguments const& arguments)
    {
        if (arguments.Positional.size() < 2)
        {
            Print("usage: rtpmidi-spike service host <name> [--port P] [--service-name LABEL] [--no-advertise] [--no-journal]");
            return 1;
        }

        rtp::MidiRtpHostConfig config;
        config.Name(arguments.Positional[1]);
        config.ServiceInstanceName(arguments.Get(L"--service-name"));
        config.Advertise(!arguments.Has(L"--no-advertise"));
        config.SendRecoveryJournal(!arguments.Has(L"--no-journal"));
        config.AllowPortFallback(true);

        if (arguments.Has(L"--port"))
        {
            uint16_t port{ 0 };
            if (!TryParsePort(arguments.Get(L"--port"), port))
            {
                Print("The port has to be a number from 1024 to 65534.");
                return 1;
            }

            config.UseAutomaticPort(false);
            config.Port(port);
        }

        Print("Starting host \"%s\"...", ToUtf8(arguments.Positional[1]).c_str());

        auto const response = rtp::MidiRtpTransportManager::CreateHostAsync(config).get();

        if (!response.Success())
        {
            Print("The host was not created: %s (error %d)", Text(response.ErrorMessage()).c_str(), static_cast<int>(response.ErrorCode()));
            return 2;
        }

        for (auto const& host : rtp::MidiRtpTransportManager::GetConfiguredHosts())
        {
            if (host.HostId() == config.HostId()) PrintHost(host);
        }

        if (config.Advertise())
        {
            Print("");
            Print("On a Mac it appears in Audio MIDI Setup, in MIDI Network Setup's Directory list.");
        }

        return 0;
    }

    // An advertised name as the service sees it, from what was typed. Exact matches win, then a
    // name which contains what was typed, as long as only one does.
    bool TryMatchPeer(std::wstring const& typed, rtp::MidiRtpAdvertisedPeer& match, bool& ambiguous)
    {
        match = nullptr;
        ambiguous = false;

        auto const wanted = Fold(typed);
        std::vector<rtp::MidiRtpAdvertisedPeer> partial;

        for (auto const& peer : rtp::MidiRtpTransportManager::GetAdvertisedPeers())
        {
            auto const name = Fold(std::wstring{ peer.ServiceInstanceName() });

            if (name == wanted)
            {
                match = peer;
                return true;
            }

            if (name.find(wanted) != std::wstring::npos) partial.push_back(peer);
        }

        if (partial.size() == 1)
        {
            match = partial.front();
            return true;
        }

        if (partial.size() > 1)
        {
            ambiguous = true;

            Print("More than one advertised device matches \"%s\":", ToUtf8(typed).c_str());
            for (auto const& peer : partial) Print("  \"%s\"", Text(peer.ServiceInstanceName()).c_str());
        }

        return false;
    }

    // "address:port", "host.local:port" or "[IPv6 address]:port"
    bool TrySplitAddress(std::wstring const& target, std::wstring& address, uint16_t& port)
    {
        address.clear();
        port = 0;

        if (!target.empty() && target.front() == L'[')
        {
            auto const close = target.find(L"]:");
            if (close == std::wstring::npos) return false;

            address = target.substr(1, close - 1);
            return TryParsePort(target.substr(close + 2), port);
        }

        auto const colon = target.rfind(L':');
        if (colon == std::wstring::npos || target.find(L':') != colon) return false;

        address = target.substr(0, colon);
        return !address.empty() && TryParsePort(target.substr(colon + 1), port);
    }

    int CommandConnect(Arguments const& arguments)
    {
        if (arguments.Positional.size() < 2)
        {
            Print("usage: rtpmidi-spike service connect <advertised name | address:port> [--name NAME] [--endpoint-name NAME] [--no-reconnect] [--no-journal]");
            return 1;
        }

        auto const& target = arguments.Positional[1];

        std::wstring ourName = arguments.Get(L"--name");
        if (ourName.empty())
        {
            wchar_t computerName[256]{};
            DWORD size = ARRAYSIZE(computerName);
            GetComputerNameExW(ComputerNameDnsHostname, computerName, &size);
            ourName = computerName;
        }

        rtp::MidiRtpClientConfig config;
        config.Name(ourName);
        config.CustomEndpointName(arguments.Get(L"--endpoint-name"));
        config.AutoReconnect(!arguments.Has(L"--no-reconnect"));
        config.SendRecoveryJournal(!arguments.Has(L"--no-journal"));

        std::wstring address;
        uint16_t port{ 0 };

        if (TrySplitAddress(target, address, port))
        {
            config.RemoteAddress(address);
            config.RemotePort(port);
            Print("Connecting to %s port %u as \"%s\"...", ToUtf8(address).c_str(), port, ToUtf8(ourName).c_str());
        }
        else
        {
            rtp::MidiRtpAdvertisedPeer peer{ nullptr };
            bool ambiguous{ false };

            if (TryMatchPeer(target, peer, ambiguous))
            {
                config.RemoteServiceInstanceName(peer.ServiceInstanceName());
                Print("Connecting to \"%s\" as \"%s\"...%s", Text(peer.ServiceInstanceName()).c_str(), ToUtf8(ourName).c_str(),
                    peer.IsThisPc() ? " (a host on this PC)" : "");
            }
            else if (ambiguous)
            {
                Print("Nothing was created. Type the whole name, in quotes if it has spaces.");
                return 1;
            }
            else
            {
                config.RemoteServiceInstanceName(target);
                Print("\"%s\" is not advertised right now. The service connects when it appears.", ToUtf8(target).c_str());
            }
        }

        auto const response = rtp::MidiRtpTransportManager::ConnectClientAsync(config).get();

        if (!response.Success())
        {
            Print("The connection was not created: %s (error %d)", Text(response.ErrorMessage()).c_str(), static_cast<int>(response.ErrorCode()));
            return 2;
        }

        // an invitation is retried for twelve seconds before the remote counts as not answering
        auto const end = GetTickCount64() + 15000;
        rtp::MidiRtpConfiguredClient client{ nullptr };

        while (GetTickCount64() < end && !Spike::StopRequested())
        {
            for (auto const& candidate : rtp::MidiRtpTransportManager::GetConfiguredClients())
            {
                if (candidate.ClientId() == config.ClientId()) client = candidate;
            }

            if (client != nullptr && client.EntryState() != rtp::MidiRtpClientEntryState::Pending) break;

            Sleep(250);
        }

        if (client == nullptr)
        {
            Print("The service accepted the entry but does not report it. Try \"service status\".");
            return 2;
        }

        PrintClient(client);

        if (client.EntryState() == rtp::MidiRtpClientEntryState::Active) return 0;

        if (static_cast<uint32_t>(client.LastErrorCode()) == 0x80070005)
        {
            Print("");
            Print("On a Mac, set \"Who may connect to me\" to Anyone in MIDI Network Setup, and make sure the session is enabled.");
        }

        if (client.EntryState() != rtp::MidiRtpClientEntryState::Unavailable)
        {
            Print("The service keeps trying. Stop it with: rtpmidi-spike service remove %s", ToUtf8(GuidText(client.ClientId()).substr(1, 8)).c_str());
        }

        return 2;
    }

    int CommandRemove(Arguments const& arguments)
    {
        if (arguments.Positional.size() < 2)
        {
            Print("usage: rtpmidi-spike service remove <entry id, its first few characters, or all>");
            return 1;
        }

        auto const wanted = IdKey(arguments.Positional[1]);
        bool const all = wanted == L"all";

        struct Entry
        {
            winrt::guid Id{};
            bool IsHost{ false };
            std::string Name;
        };

        std::vector<Entry> matches;

        for (auto const& host : rtp::MidiRtpTransportManager::GetConfiguredHosts())
        {
            if (all || IdKey(GuidText(host.HostId())).rfind(wanted, 0) == 0) matches.push_back({ host.HostId(), true, Text(host.Name()) });
        }

        for (auto const& client : rtp::MidiRtpTransportManager::GetConfiguredClients())
        {
            if (all || IdKey(GuidText(client.ClientId())).rfind(wanted, 0) == 0) matches.push_back({ client.ClientId(), false, Text(client.Name()) });
        }

        if (matches.empty())
        {
            Print("Nothing matches \"%s\". \"service status\" lists the entries.", ToUtf8(arguments.Positional[1]).c_str());
            return 1;
        }

        if (matches.size() > 1 && !all)
        {
            Print("\"%s\" matches more than one entry. Type more of the id:", ToUtf8(arguments.Positional[1]).c_str());
            for (auto const& match : matches) Print("  %s  %s \"%s\"", ToUtf8(GuidText(match.Id)).c_str(), match.IsHost ? "host" : "connection", match.Name.c_str());
            return 1;
        }

        int result = 0;

        for (auto const& match : matches)
        {
            auto const response = match.IsHost ?
                rtp::MidiRtpTransportManager::RemoveHostAsync(match.Id).get() :
                rtp::MidiRtpTransportManager::RemoveClientAsync(match.Id).get();

            if (response.Success())
            {
                Print("Removed %s \"%s\"", match.IsHost ? "host" : "connection", match.Name.c_str());
            }
            else
            {
                Print("Could not remove \"%s\": %s", match.Name.c_str(), Text(response.ErrorMessage()).c_str());
                result = 2;
            }
        }

        return result;
    }

    struct Arrival
    {
        uint64_t ArrivedAt{ 0 };
        uint64_t Timestamp{ 0 };
        uint32_t Words[4]{};
    };

    // Separate from the connection so the handler can hold it without a reference cycle
    struct Inbox
    {
        std::mutex Lock;
        std::vector<Arrival> Arrivals;
    };

    struct Listener
    {
        std::wstring EndpointId;
        std::string Name;
        midi2::MidiEndpointConnection Connection{ nullptr };
        winrt::event_token Token{};
        std::shared_ptr<Inbox> Received{ std::make_shared<Inbox>() };
        bool Listening{ false };
    };

    struct SentNote
    {
        uint64_t SentAt{ 0 };
        uint8_t Note{ 0 };
        uint8_t Velocity{ 0 };
        bool On{ false };
    };

    struct SentSysEx
    {
        std::vector<uint8_t> Payload;
        uint64_t FirstSentAt{ 0 };
    };

    struct ReceivedSysEx
    {
        std::vector<uint8_t> Bytes;
        uint64_t FirstAt{ 0 };
        uint64_t LastAt{ 0 };
        bool Complete{ false };
    };

    // 0x7D is the non-commercial manufacturer id; the marker and index tell our messages apart
    constexpr uint8_t LoopTestSysExId{ 0x7D };
    constexpr uint8_t LoopTestSysExMarker{ 0x52 };
    constexpr size_t LoopTestMaxSysExSizes{ 20 };
    constexpr size_t LoopTestMaxCopies{ 4 };

    // One copy of every note, when a route and a cable both bring messages back
    struct CopyResults
    {
        size_t Matched{ 0 };
        size_t OutOfOrder{ 0 };
        size_t LastIndex{ 0 };
        std::vector<double> RoundTrip;
        std::vector<double> StampVersusArrival;
        std::set<uint32_t> Groups;
    };

    char const* CopyName(size_t const copy)
    {
        static char const* const names[LoopTestMaxCopies]{ "first copy", "second copy", "third copy", "fourth copy" };
        return copy < LoopTestMaxCopies ? names[copy] : "another copy";
    }

    double TicksToMs(uint64_t const later, uint64_t const earlier)
    {
        static double const ticksPerMillisecond = static_cast<double>(midi2::MidiClock::TimestampFrequency()) / 1000.0;
        return static_cast<double>(static_cast<int64_t>(later - earlier)) / ticksPerMillisecond;
    }

    uint64_t MsToTicks(uint64_t const milliseconds)
    {
        return midi2::MidiClock::TimestampFrequency() * milliseconds / 1000;
    }

    void WaitUntil(uint64_t const due)
    {
        for (auto now = midi2::MidiClock::Now(); now < due && !Spike::StopRequested(); now = midi2::MidiClock::Now())
        {
            if (TicksToMs(due, now) > 20.0) Sleep(10);
            else YieldProcessor();
        }
    }

    std::string Spread(std::vector<double> values)
    {
        if (values.empty()) return "none";

        std::sort(values.begin(), values.end());

        char text[128]{};
        snprintf(text, sizeof(text), "min %.2f ms, median %.2f ms, max %.2f ms", values.front(), values[values.size() / 2], values.back());
        return text;
    }

    bool TryParseNumber(std::wstring const& text, uint32_t const minimum, uint32_t const maximum, uint32_t& value)
    {
        if (text.empty() || text.size() > 9 || !std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; })) return false;

        auto const parsed = std::stoul(text);
        if (parsed < minimum || parsed > maximum) return false;

        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool TryGetNumber(Arguments const& arguments, std::wstring const& name, uint32_t const minimum, uint32_t const maximum, uint32_t& value)
    {
        if (!arguments.Has(name) || TryParseNumber(arguments.Get(name), minimum, maximum, value)) return true;

        Print("%s needs a number from %u to %u.", ToUtf8(name).c_str(), minimum, maximum);
        return false;
    }

    std::vector<std::wstring> SplitList(std::wstring const& text)
    {
        std::vector<std::wstring> items;
        size_t start{ 0 };

        while (start <= text.size())
        {
            auto const comma = text.find(L',', start);
            auto item = text.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);

            while (!item.empty() && item.front() == L' ') item.erase(item.begin());
            while (!item.empty() && item.back() == L' ') item.pop_back();
            if (!item.empty()) items.push_back(item);

            if (comma == std::wstring::npos) break;
            start = comma + 1;
        }

        return items;
    }

    // An endpoint id, an exact name, or part of a name that only one endpoint has
    bool TryFindEndpoint(std::wstring const& typed, enumeration::MidiEndpointDeviceInformation& found)
    {
        found = nullptr;

        auto const wanted = Fold(typed);
        std::vector<enumeration::MidiEndpointDeviceInformation> partial;

        for (auto const& endpoint : enumeration::MidiEndpointDeviceInformation::FindAll())
        {
            auto const name = Fold(std::wstring{ endpoint.Name() });

            if (name == wanted || Fold(std::wstring{ endpoint.EndpointDeviceId() }) == wanted)
            {
                found = endpoint;
                return true;
            }

            if (name.find(wanted) != std::wstring::npos) partial.push_back(endpoint);
        }

        if (partial.size() == 1)
        {
            found = partial.front();
            return true;
        }

        if (partial.empty())
        {
            Print("No endpoint is called \"%s\". \"midi enumerate endpoints\" lists them.", ToUtf8(typed).c_str());
        }
        else
        {
            Print("More than one endpoint matches \"%s\":", ToUtf8(typed).c_str());
            for (auto const& endpoint : partial) Print("  \"%s\"  %s", Text(endpoint.Name()).c_str(), Text(endpoint.EndpointDeviceId()).c_str());
        }

        return false;
    }

    std::vector<std::pair<uint32_t, uint32_t>> SysEx7Packets(uint32_t const groupIndex, std::vector<uint8_t> const& payload)
    {
        std::vector<std::pair<uint32_t, uint32_t>> packets;

        for (size_t offset = 0; offset < payload.size(); offset += 6)
        {
            auto const count = std::min<size_t>(6, payload.size() - offset);
            bool const first = offset == 0;
            bool const last = offset + count == payload.size();
            uint32_t const status = first && last ? 0 : first ? 1 : last ? 3 : 2;

            uint8_t data[6]{};
            std::copy_n(payload.begin() + static_cast<ptrdiff_t>(offset), count, data);

            packets.emplace_back(
                0x30000000u | (groupIndex << 24) | (status << 20) | (static_cast<uint32_t>(count) << 16) | (static_cast<uint32_t>(data[0]) << 8) | data[1],
                (static_cast<uint32_t>(data[2]) << 24) | (static_cast<uint32_t>(data[3]) << 16) | (static_cast<uint32_t>(data[4]) << 8) | data[5]);
        }

        return packets;
    }

    // SysEx7 packets back into whole messages, per group, in arrival order
    std::vector<ReceivedSysEx> ReassembleSysEx(std::vector<Arrival> const& arrivals)
    {
        std::vector<ReceivedSysEx> messages;
        std::map<uint32_t, size_t> open;

        for (auto const& arrival : arrivals)
        {
            auto const word0 = arrival.Words[0];
            auto const word1 = arrival.Words[1];
            if ((word0 >> 28) != 0x3) continue;

            auto const group = (word0 >> 24) & 0x0F;
            auto const status = (word0 >> 20) & 0x0F;
            auto const count = std::min<uint32_t>((word0 >> 16) & 0x0F, 6);
            uint8_t const data[6]{ static_cast<uint8_t>(word0 >> 8), static_cast<uint8_t>(word0), static_cast<uint8_t>(word1 >> 24),
                static_cast<uint8_t>(word1 >> 16), static_cast<uint8_t>(word1 >> 8), static_cast<uint8_t>(word1) };

            auto current = open.find(group);

            if (status == 0 || status == 1 || current == open.end())
            {
                messages.push_back({});
                messages.back().FirstAt = arrival.ArrivedAt;
                current = open.insert_or_assign(group, messages.size() - 1).first;
            }

            auto& message = messages[current->second];
            message.Bytes.insert(message.Bytes.end(), data, data + count);
            message.LastAt = arrival.ArrivedAt;

            if (status == 0 || status == 3)
            {
                message.Complete = true;
                open.erase(current);
            }
        }

        return messages;
    }

    void ReportLoopTest(Listener const& listener, std::vector<SentNote> const& sentNotes, std::vector<SentSysEx> const& sentSysEx)
    {
        std::vector<Arrival> arrivals;
        {
            auto lock = std::scoped_lock{ listener.Received->Lock };
            arrivals = listener.Received->Arrivals;
        }

        Print("");
        Print("\"%s\" (%zu messages arrived)", listener.Name.c_str(), arrivals.size());

        std::vector<size_t> copies(sentNotes.size(), 0);
        std::vector<CopyResults> results;
        std::vector<bool> accounted(arrivals.size(), false);

        for (size_t a = 0; a < arrivals.size(); a++)
        {
            auto const word0 = arrivals[a].Words[0];
            if ((word0 >> 28) != 0x2) continue;

            auto const status = (word0 >> 16) & 0xF0;
            auto const note = static_cast<uint8_t>((word0 >> 8) & 0x7F);
            auto const velocity = static_cast<uint8_t>(word0 & 0x7F);
            bool const on = status == 0x90 && velocity != 0;

            // a device may send Note Off as Note On with velocity 0, which drops the release velocity
            if (!on && status != 0x80 && status != 0x90) continue;

            size_t best{ sentNotes.size() };

            for (size_t i = 0; i < sentNotes.size(); i++)
            {
                if (sentNotes[i].On != on || sentNotes[i].Note != note || arrivals[a].ArrivedAt < sentNotes[i].SentAt) continue;
                if ((on || status == 0x80) && sentNotes[i].Velocity != velocity) continue;

                if (best == sentNotes.size() || copies[i] < copies[best]) best = i;
            }

            if (best == sentNotes.size() || copies[best] >= LoopTestMaxCopies) continue;

            auto const copy = copies[best]++;
            if (results.size() <= copy) results.resize(copy + 1);

            auto& result = results[copy];
            if (result.Matched > 0 && best < result.LastIndex) result.OutOfOrder++;

            result.LastIndex = best;
            result.Matched++;
            result.RoundTrip.push_back(TicksToMs(arrivals[a].ArrivedAt, sentNotes[best].SentAt));
            result.StampVersusArrival.push_back(TicksToMs(arrivals[a].Timestamp, arrivals[a].ArrivedAt));
            result.Groups.insert((word0 >> 24) & 0x0F);

            accounted[a] = true;
        }

        if (!sentNotes.empty() && results.empty()) Print("  notes: none of %zu came back", sentNotes.size());

        for (size_t c = 0; c < results.size(); c++)
        {
            auto const& result = results[c];

            std::string groupText;
            for (auto const group : result.Groups) groupText += (groupText.empty() ? ", on group " : ", ") + std::to_string(group + 1);

            Print("  notes, %s: %zu of %zu%s%s", CopyName(c), result.Matched, sentNotes.size(), groupText.c_str(),
                result.Matched < 2 ? "" : result.OutOfOrder == 0 ? ", in order" : ", OUT OF ORDER");
            Print("    round trip: %s", Spread(result.RoundTrip).c_str());
            Print("    timestamp minus arrival (above 0 is stamped later than it arrived): %s", Spread(result.StampVersusArrival).c_str());
        }

        auto const received = ReassembleSysEx(arrivals);
        std::vector<bool> sysexUsed(received.size(), false);

        for (size_t k = 0; k < sentSysEx.size(); k++)
        {
            auto const& expected = sentSysEx[k].Payload;
            size_t copy{ 0 };

            for (size_t r = 0; r < received.size(); r++)
            {
                auto const& got = received[r];
                auto const& bytes = got.Bytes;
                if (bytes.size() < 3 || bytes[0] != LoopTestSysExId || bytes[1] != LoopTestSysExMarker || bytes[2] != static_cast<uint8_t>(k)) continue;

                sysexUsed[r] = true;

                auto const firstBack = TicksToMs(got.FirstAt, sentSysEx[k].FirstSentAt);
                auto const span = TicksToMs(got.LastAt, got.FirstAt);

                if (got.Complete && bytes == expected)
                {
                    Print("  SysEx of %zu bytes, %s: identical. First bytes back after %.1f ms, the rest over %.0f ms",
                        expected.size(), CopyName(copy), firstBack, span);
                }
                else
                {
                    size_t same{ 0 };
                    while (same < bytes.size() && same < expected.size() && bytes[same] == expected[same]) same++;

                    Print("  SysEx of %zu bytes, %s: DIFFERENT. %zu bytes came back%s; the first %zu match",
                        expected.size(), CopyName(copy), bytes.size(), got.Complete ? "" : " with no end", same);
                }

                copy++;
            }

            if (copy == 0) Print("  SysEx of %zu bytes: did not come back", expected.size());
        }

        size_t otherSysEx{ 0 };
        for (size_t r = 0; r < received.size(); r++) if (!sysexUsed[r]) otherSysEx++;

        size_t others{ 0 };
        std::string examples;

        for (size_t a = 0; a < arrivals.size(); a++)
        {
            if (accounted[a] || (arrivals[a].Words[0] >> 28) == 0x3) continue;

            if (others++ < 5)
            {
                char text[16]{};
                snprintf(text, sizeof(text), "%08X", arrivals[a].Words[0]);
                examples += (examples.empty() ? " (" : " ") + std::string{ text };
            }
        }

        if (others == 0 && otherSysEx == 0) Print("  nothing else arrived");
        else Print("  also arrived: %zu other messages%s%s, %zu other SysEx", others, examples.c_str(), examples.empty() ? "" : others > 5 ? " ...)" : ")", otherSysEx);
    }

    int CommandLoopTest(Arguments const& arguments)
    {
        if (arguments.Positional.size() < 2)
        {
            Print("usage: rtpmidi-spike service loop-test <send endpoint> [--listen ENDPOINT,ENDPOINT] [--group 1-16] [--notes N] [--interval MS] [--sysex BYTES,BYTES | none] [--pace-us US]");
            return 1;
        }

        uint32_t group{ 1 };
        uint32_t noteCount{ 20 };
        uint32_t intervalMs{ 25 };
        uint32_t paceMicroseconds{ 2500 };

        if (!TryGetNumber(arguments, L"--group", 1, 16, group) ||
            !TryGetNumber(arguments, L"--notes", 0, 1000, noteCount) ||
            !TryGetNumber(arguments, L"--interval", 1, 10000, intervalMs) ||
            !TryGetNumber(arguments, L"--pace-us", 0, 1000000, paceMicroseconds))
        {
            return 1;
        }

        std::vector<uint32_t> sysexSizes;
        auto const sizeText = arguments.Has(L"--sysex") ? arguments.Get(L"--sysex") : std::wstring{ L"16,256,2048" };

        for (auto const& item : SplitList(sizeText == L"none" ? std::wstring{} : sizeText))
        {
            uint32_t size{ 0 };

            if (!TryParseNumber(item, 3, 65536, size) || sysexSizes.size() == LoopTestMaxSysExSizes)
            {
                Print("--sysex takes up to %zu sizes, each 3 to 65536 bytes between F0 and F7, or none.", LoopTestMaxSysExSizes);
                return 1;
            }

            sysexSizes.push_back(size);
        }

        enumeration::MidiEndpointDeviceInformation sendEndpoint{ nullptr };
        if (!TryFindEndpoint(arguments.Positional[1], sendEndpoint)) return 1;

        std::vector<enumeration::MidiEndpointDeviceInformation> listenEndpoints;

        for (auto const& typed : SplitList(arguments.Get(L"--listen")))
        {
            enumeration::MidiEndpointDeviceInformation endpoint{ nullptr };
            if (!TryFindEndpoint(typed, endpoint)) return 1;

            listenEndpoints.push_back(endpoint);
        }

        if (listenEndpoints.empty()) listenEndpoints.push_back(sendEndpoint);

        auto session = midi2::MidiSession::Create(L"rtpmidi-spike loop test");
        if (session == nullptr)
        {
            Print("Could not create a MIDI session.");
            return 3;
        }

        std::vector<std::unique_ptr<Listener>> listeners;

        auto connectionFor = [&](enumeration::MidiEndpointDeviceInformation const& endpoint) -> Listener*
        {
            std::wstring const id{ endpoint.EndpointDeviceId() };

            for (auto const& existing : listeners)
            {
                if (existing->EndpointId == id) return existing.get();
            }

            auto connection = session.CreateEndpointConnection(endpoint.EndpointDeviceId());
            if (connection == nullptr) return nullptr;

            auto listener = std::make_unique<Listener>();
            listener->EndpointId = id;
            listener->Name = Text(endpoint.Name());
            listener->Connection = connection;

            listeners.push_back(std::move(listener));
            return listeners.back().get();
        };

        auto* const sender = connectionFor(sendEndpoint);
        bool ready = sender != nullptr;

        for (auto const& endpoint : listenEndpoints)
        {
            auto* const listener = ready ? connectionFor(endpoint) : nullptr;
            if (listener == nullptr || listener->Listening)
            {
                ready = ready && listener != nullptr;
                continue;
            }

            listener->Listening = true;
            listener->Token = listener->Connection.MessageReceived([inbox = listener->Received](midi2::IMidiMessageReceivedEventSource const&, midi2::MidiMessageReceivedEventArgs const& args)
            {
                try
                {
                    Arrival arrival{};
                    arrival.ArrivedAt = midi2::MidiClock::Now();
                    arrival.Timestamp = args.Timestamp();
                    args.FillWords(arrival.Words[0], arrival.Words[1], arrival.Words[2], arrival.Words[3]);

                    auto lock = std::scoped_lock{ inbox->Lock };
                    inbox->Arrivals.push_back(arrival);
                }
                catch (...)
                {
                }
            });
        }

        for (auto const& listener : listeners)
        {
            if (ready && !listener->Connection.Open())
            {
                Print("Could not open \"%s\".", listener->Name.c_str());
                ready = false;
            }
        }

        std::vector<SentNote> sentNotes;
        std::vector<SentSysEx> sentSysEx;
        size_t sendFailures{ 0 };

        if (ready)
        {
            std::string listening;
            for (auto const& listener : listeners)
            {
                if (listener->Listening) listening += (listening.empty() ? "\"" : ", \"") + listener->Name + "\"";
            }

            Print("Sending through \"%s\" on group %u: %u notes, %u ms apart, then %zu SysEx messages%s.", sender->Name.c_str(), group, noteCount, intervalMs,
                sysexSizes.size(), paceMicroseconds == 0 ? " as fast as possible" : ", 6 bytes at a time");
            Print("Listening on %s.", listening.c_str());

            auto const groupIndex = group - 1;
            auto const sendNow = midi2::MidiClock::TimestampConstantSendImmediately();

            WaitUntil(midi2::MidiClock::Now() + MsToTicks(300));

            auto due = midi2::MidiClock::Now();

            for (uint32_t i = 0; i < noteCount * 2 && !Spike::StopRequested(); i++)
            {
                auto const index = i / 2;
                bool const on = (i % 2) == 0;
                auto const note = static_cast<uint8_t>(36 + index % 60);
                auto const velocity = static_cast<uint8_t>(1 + (index / 60) % 127);
                auto const word = 0x20000000u | (groupIndex << 24) | (static_cast<uint32_t>(on ? 0x90 : 0x80) << 16) | (static_cast<uint32_t>(note) << 8) | velocity;

                WaitUntil(due);

                sentNotes.push_back({ midi2::MidiClock::Now(), note, velocity, on });
                if (!midi2::MidiEndpointConnection::SendMessageSucceeded(sender->Connection.SendSingleMessageWords(sendNow, word))) sendFailures++;

                due += MsToTicks(intervalMs);
            }

            WaitUntil(midi2::MidiClock::Now() + MsToTicks(500));

            auto const pace = midi2::MidiClock::TimestampFrequency() * paceMicroseconds / 1000000;

            for (size_t k = 0; k < sysexSizes.size() && !Spike::StopRequested(); k++)
            {
                SentSysEx message;
                message.Payload.resize(sysexSizes[k]);
                message.Payload[0] = LoopTestSysExId;
                message.Payload[1] = LoopTestSysExMarker;
                message.Payload[2] = static_cast<uint8_t>(k);
                for (size_t b = 3; b < message.Payload.size(); b++) message.Payload[b] = static_cast<uint8_t>((b * 37 + k * 11) & 0x7F);

                due = midi2::MidiClock::Now();
                message.FirstSentAt = due;

                for (auto const& [word0, word1] : SysEx7Packets(groupIndex, message.Payload))
                {
                    WaitUntil(due);
                    if (!midi2::MidiEndpointConnection::SendMessageSucceeded(sender->Connection.SendSingleMessageWords(sendNow, word0, word1))) sendFailures++;
                    due += pace;
                }

                sentSysEx.push_back(std::move(message));

                // time for a 5-pin DIN link to carry it: 10 bits a byte at 31,250 baud
                WaitUntil(midi2::MidiClock::Now() + MsToTicks(300 + static_cast<uint64_t>(sysexSizes[k]) * 10 * 1000 / 31250));
            }

            WaitUntil(midi2::MidiClock::Now() + MsToTicks(1000));
        }

        for (auto const& listener : listeners)
        {
            if (listener->Listening) listener->Connection.MessageReceived(listener->Token);
        }

        session.Close();

        if (!ready) return 3;

        if (sendFailures > 0) Print("%zu messages could not be sent.", sendFailures);

        for (auto const& listener : listeners)
        {
            if (listener->Listening) ReportLoopTest(*listener, sentNotes, sentSysEx);
        }

        return 0;
    }
}


int RunServiceCommand(std::vector<std::wstring> const& raw)
{
    auto const arguments = Parse(raw);
    auto const verb = arguments.Positional.empty() ? std::wstring{} : arguments.Positional.front();

    if (verb != L"status" && verb != L"host" && verb != L"connect" && verb != L"remove" && verb != L"loop-test")
    {
        Print("rtpmidi-spike service status | host <name> | connect <advertised name | address:port> | remove <id | all> | loop-test <endpoint>");
        Print("See the comment at the top of spike_service.cpp for the options.");
        return 1;
    }

    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        wchar_t exePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exePath, ARRAYSIZE(exePath));

        std::wstring sdkDll{ exePath };
        sdkDll = sdkDll.substr(0, sdkDll.find_last_of(L'\\') + 1) + L"sdk\\Windows.Devices.Midi2.Transports.Rtp.dll";

        if (!SdkCheckStart(sdkDll))
        {
            Print("Could not load %s. Build rtp-midi.sln first.", ToUtf8(sdkDll).c_str());
            return 3;
        }

        // Registered on some PCs, app-local on others, so a missing copy is not fatal yet
        auto const midi2Runtime = SdkLoadMidi2Runtime();
        if (arguments.Has(L"--verbose")) Print("Windows MIDI Services SDK: %s", midi2Runtime.empty() ? "registered copy" : ToUtf8(midi2Runtime).c_str());

        if (!midi2::MidiApi::EnsureServiceAvailable())
        {
            Print("The MIDI service is not available.");
            return 3;
        }

        if (!rtp::MidiRtpTransportManager::IsTransportAvailable())
        {
            Print("The RTP-MIDI transport is not installed in the MIDI service. Run register-dev-transport.cmd as administrator.");
            return 3;
        }

        if (verb == L"status") return CommandStatus();
        if (verb == L"host") return CommandHost(arguments);
        if (verb == L"connect") return CommandConnect(arguments);
        if (verb == L"loop-test") return CommandLoopTest(arguments);
        return CommandRemove(arguments);
    }
    catch (winrt::hresult_error const& error)
    {
        Print("Failed: %s (0x%08X)", Text(error.message()).c_str(), static_cast<uint32_t>(error.code()));
        return 4;
    }
    catch (std::exception const& error)
    {
        Print("Failed: %s", error.what());
        return 4;
    }
}
