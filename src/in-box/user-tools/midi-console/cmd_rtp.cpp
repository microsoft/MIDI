// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "cmd_rtp.h"
#include "console_output.h"
#include "console_table.h"
#include "midi_formatting.h"
#include "return_codes.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        bool EnsureTransportAvailable()
        {
            if (midi2rtp::MidiRtpTransportManager::IsTransportAvailable())
            {
                return true;
            }

            WriteErrorLine(ResourceString(IDS_RTP_NOT_AVAILABLE));

            return false;
        }

        std::string FormatRemoteClientPolicy(_In_ midi2rtp::MidiRtpRemoteClientPolicy const policy)
        {
            switch (policy)
            {
            case midi2rtp::MidiRtpRemoteClientPolicy::AllowAny:        return ResourceString(IDS_RTP_POLICY_ALLOW_ANY);
            case midi2rtp::MidiRtpRemoteClientPolicy::RequireApproval: return ResourceString(IDS_RTP_POLICY_REQUIRE_APPROVAL);
            default:                                                   return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }

        std::string FormatClientEntryState(_In_ midi2rtp::MidiRtpClientEntryState const state)
        {
            switch (state)
            {
            case midi2rtp::MidiRtpClientEntryState::Pending:     return ResourceString(IDS_RTP_STATE_PENDING);
            case midi2rtp::MidiRtpClientEntryState::Active:      return ResourceString(IDS_RTP_STATE_ACTIVE);
            case midi2rtp::MidiRtpClientEntryState::Retrying:    return ResourceString(IDS_RTP_STATE_RETRYING);
            case midi2rtp::MidiRtpClientEntryState::Unavailable: return ResourceString(IDS_RTP_STATE_UNAVAILABLE);
            default:                                             return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }

        std::string FormatHresult(_In_ int32_t const value)
        {
            return fmt::format("0x{:08X}", static_cast<uint32_t>(value));
        }

        // Empty when the last attempt did not fail
        std::string DescribeClientProblem(_In_ int32_t const lastErrorCode)
        {
            auto const hr = static_cast<HRESULT>(lastErrorCode);

            if (hr == S_OK)
            {
                return {};
            }

            if (hr == HRESULT_FROM_WIN32(ERROR_HOST_UNREACHABLE))
            {
                return ResourceString(IDS_RTP_PROBLEM_NOT_FOUND);
            }

            if (hr == HRESULT_FROM_WIN32(ERROR_TIMEOUT))
            {
                return ResourceString(IDS_RTP_PROBLEM_NO_ANSWER);
            }

            if (hr == E_ACCESSDENIED)
            {
                return ResourceString(IDS_RTP_PROBLEM_REFUSED);
            }

            if (hr == HRESULT_FROM_WIN32(ERROR_GRACEFUL_DISCONNECT))
            {
                return ResourceString(IDS_RTP_PROBLEM_ENDED);
            }

            if (hr == HRESULT_FROM_WIN32(ERROR_CONNECTION_ABORTED))
            {
                return ResourceString(IDS_RTP_PROBLEM_LOST);
            }

            return FormatResourceString(IDS_RTP_PROBLEM_OTHER, FormatHresult(lastErrorCode));
        }

        // Latency is reported in MIDI clock ticks, which mean nothing without the frequency.
        std::string FormatLatency(_In_ uint64_t const ticks)
        {
            if (ticks == 0)
            {
                return ResourceString(IDS_LABEL_UNKNOWN);
            }

            return fmt::format("{:.3f} ms", midi2::MidiClock::ConvertTimestampTicksToMilliseconds(ticks));
        }

        // An IPv6 address is full of colons, so without brackets the port is unreadable.
        std::string FormatAddressAndPort(_In_ winrt::hstring const& address, _In_ uint16_t const port)
        {
            auto const addressText = ToUtf8(address);

            if (addressText.empty())
            {
                return port == 0 ? ResourceString(IDS_LABEL_NONE) : fmt::format(":{}", port);
            }

            if (addressText.find(':') != std::string::npos)
            {
                return fmt::format("[{}]:{}", addressText, port);
            }

            return fmt::format("{}:{}", addressText, port);
        }

        std::string FormatPackets(_In_ midi2rtp::MidiRtpConnection const& connection)
        {
            return fmt::format("{} / {}",
                connection.TotalCountNetworkPacketsReceived(), connection.TotalCountNetworkPacketsSent());
        }

        // Detail text has to stay unstyled: the table measures it as plain text.
        std::string DetailLine(_In_ UINT const labelId, _In_ std::string const& value)
        {
            return fmt::format("{}  {}", PadRightToWidth(ResourceString(labelId), 30), value);
        }

        // RTP-MIDI has no retransmission, so these are what describes how well a connection copes
        void AddConnectionDetails(_In_ ConsoleTable& table, _In_ midi2rtp::MidiRtpConnection const& connection)
        {
            if (!connection.RemoteHostName().empty())
            {
                table.AddRowDetail(DetailLine(IDS_NET_LABEL_HOST_NAME, ToUtf8(connection.RemoteHostName())), fieldValueTextStyle);
            }

            table.AddRowDetail(DetailLine(IDS_RTP_LABEL_LOCAL_PORT, fmt::format("{}", connection.LocalPort())), fieldValueTextStyle);
            table.AddRowDetail(DetailLine(IDS_RTP_LABEL_BEST_LATENCY, FormatLatency(connection.BestLatencyTicks())), fieldValueTextStyle);
            table.AddRowDetail(DetailLine(IDS_RTP_LABEL_MESSAGES,
                fmt::format("{} / {}", connection.TotalCountMessagesReceived(), connection.TotalCountMessagesSent())),
                fieldValueTextStyle);
            table.AddRowDetail(DetailLine(IDS_RTP_LABEL_LOSSES,
                fmt::format("{} / {} / {}",
                    connection.TotalCountPacketsLost(),
                    connection.TotalCountLossesRepairedFromJournal(),
                    connection.TotalCountNoteOffsRecovered())),
                fieldValueTextStyle);
        }

        // The port a host asked for: the configured one, or the default when it is automatic
        std::string WantedPort(_In_ midi2rtp::MidiRtpConfiguredHost const& host)
        {
            auto const configured = ToUtf8(host.ConfiguredPort());

            bool const isNumber = !configured.empty() &&
                std::all_of(configured.begin(), configured.end(), [](char const c) { return c >= '0' && c <= '9'; });

            return isNumber ? configured : fmt::format("{}", midi2rtp::MidiRtpTransportManager::DefaultHostPort());
        }

        std::string HostDisplayName(_In_ midi2rtp::MidiRtpConfiguredHost const& host)
        {
            if (!host.Name().empty())
            {
                return ToUtf8(host.Name());
            }

            return ToUtf8(host.ActualServiceInstanceName().empty() ? host.ServiceInstanceName() : host.ActualServiceInstanceName());
        }

        // The name the customer gave the endpoint, then what the remote is called, then where it is
        std::string ClientDisplayName(_In_ midi2rtp::MidiRtpConfiguredClient const& client)
        {
            if (!client.CustomEndpointName().empty())
            {
                return ToUtf8(client.CustomEndpointName());
            }

            if (!client.RemoteServiceInstanceName().empty())
            {
                return ToUtf8(client.RemoteServiceInstanceName());
            }

            if (auto const connection = client.Connection(); connection != nullptr && !connection.RemoteName().empty())
            {
                return ToUtf8(connection.RemoteName());
            }

            return FormatAddressAndPort(client.ConfiguredDirectAddress(), client.ConfiguredDirectPort());
        }

        std::string ClientTarget(_In_ midi2rtp::MidiRtpConfiguredClient const& client)
        {
            if (client.IsDirectConnection())
            {
                return FormatAddressAndPort(client.ConfiguredDirectAddress(), client.ConfiguredDirectPort());
            }

            return ToUtf8(client.RemoteServiceInstanceName());
        }

        void WriteHostConnections(_In_ midi2rtp::MidiRtpConfiguredHost const& host, _In_ bool const verbose)
        {
            auto const connections = host.Connections();

            if (connections == nullptr || connections.Size() == 0)
            {
                return;
            }

            ConsoleTable table{ ResourceString(IDS_NET_CONNECTIONS_TABLE_TITLE) };

            table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
            table.SetLastColumnShrinkable();
            table.AddColumn(ResourceString(IDS_NET_LABEL_REMOTE), ColumnAlignment::Left, fieldValueTextStyle);
            table.AddColumn(ResourceString(IDS_RTP_LABEL_CONNECTED));
            table.AddColumn(ResourceString(IDS_NET_LABEL_LATENCY), ColumnAlignment::Right, numberTextStyle);
            table.AddColumn(ResourceString(IDS_RTP_LABEL_MIDI_PACKETS), ColumnAlignment::Right, numberTextStyle);

            for (auto const& connection : connections)
            {
                if (connection == nullptr)
                {
                    continue;
                }

                table.BeginRow();
                table.AddCell(ToUtf8(connection.RemoteName()));
                table.AddCell(FormatAddressAndPort(connection.RemoteAddress(), connection.RemotePort()));
                table.AddCell(FormatBoolean(connection.IsConnected()), BooleanStyle(connection.IsConnected()));
                table.AddCell(FormatLatency(connection.CurrentLatencyTicks()));
                table.AddCell(FormatPackets(connection));

                if (!connection.EndpointDeviceId().empty())
                {
                    table.AddRowDetail(ToUtf8(connection.EndpointDeviceId()), endpointIdTextStyle);
                }

                if (verbose)
                {
                    AddConnectionDetails(table, connection);
                }
            }

            table.Render();
        }

        void WriteKnownClients(_In_ midi2rtp::MidiRtpConfiguredHost const& host)
        {
            auto const known = host.KnownRemoteClients();

            if (known == nullptr || known.Size() == 0)
            {
                return;
            }

            ConsoleTable table{ ResourceString(IDS_RTP_KNOWN_CLIENTS_TABLE_TITLE) };

            table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
            table.SetLastColumnShrinkable();
            table.AddColumn(ResourceString(IDS_RTP_LABEL_DECISION));

            for (auto const& client : known)
            {
                if (client == nullptr)
                {
                    continue;
                }

                table.BeginRow();
                table.AddCell(ToUtf8(client.RemoteClientName()));
                table.AddCell(
                    ResourceString(client.IsAllowed() ? IDS_RTP_DECISION_ALLOWED : IDS_RTP_DECISION_BLOCKED),
                    client.IsAllowed() ? successTextStyle : warningTextStyle);
            }

            table.Render();
        }
    }

    int RunRtpHostsCommand(_In_ RtpListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const hosts = midi2rtp::MidiRtpTransportManager::GetConfiguredHosts();

        if (hosts == nullptr || hosts.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_RTP_NO_HOSTS));
            return 0;
        }

        for (auto const& host : hosts)
        {
            if (host == nullptr)
            {
                continue;
            }

            WriteSectionHeading(HostDisplayName(host));

            WriteField(ResourceString(IDS_NET_LABEL_HOST_ID), FormatGuid(host.HostId()), guidTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_ENABLED),
                FormatBoolean(host.IsEnabled()), BooleanStyle(host.IsEnabled()));
            WriteField(ResourceString(IDS_NET_LABEL_STARTED),
                FormatBoolean(host.HasStarted()), BooleanStyle(host.HasStarted()));
            WriteField(ResourceString(IDS_NET_LABEL_PORT),
                host.ActualPort() == 0 ? ResourceString(IDS_LABEL_NONE) : fmt::format("{}", host.ActualPort()),
                fieldValueTextStyle);

            if (host.UsedPortFallback())
            {
                WriteField(ResourceString(IDS_NET_LABEL_CONFIGURED_PORT), ToUtf8(host.ConfiguredPort()), warningTextStyle);
            }
            else if (options.Verbose)
            {
                WriteField(ResourceString(IDS_NET_LABEL_CONFIGURED_PORT), ToUtf8(host.ConfiguredPort()), fieldValueTextStyle);
            }

            WriteField(ResourceString(IDS_NET_LABEL_NETWORK_ADAPTER),
                FormatNetworkAdapter(host.NetworkAdapterId(), host.NetworkAdapterName(), host.IsNetworkAdapterMissing()),
                host.IsNetworkAdapterMissing() ? warningTextStyle : fieldValueTextStyle);

            WriteField(ResourceString(IDS_NET_LABEL_SERVICE_INSTANCE),
                !host.Advertise() ?
                    ResourceString(IDS_RTP_NOT_ADVERTISED) :
                    ToUtf8(host.ActualServiceInstanceName().empty() ? host.ServiceInstanceName() : host.ActualServiceInstanceName()),
                fieldValueTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_REMOTE_POLICY),
                FormatRemoteClientPolicy(host.RemoteClientPolicy()), fieldValueTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_SEND_SPEED),
                FormatSendSpeedLimit(static_cast<uint32_t>(host.SendSpeedLimit())), fieldValueTextStyle);

            if (host.LastErrorCode() != 0)
            {
                WriteField(ResourceString(IDS_RTP_LABEL_LAST_ERROR), FormatHresult(host.LastErrorCode()), warningTextStyle);
            }

            if (options.Verbose)
            {
                WriteField(ResourceString(IDS_RTP_LABEL_ADVERTISE),
                    FormatBoolean(host.Advertise()), BooleanStyle(host.Advertise()));
                WriteField(ResourceString(IDS_RTP_LABEL_ALLOW_PORT_FALLBACK),
                    FormatBoolean(host.AllowPortFallback()), BooleanStyle(host.AllowPortFallback()));
                WriteField(ResourceString(IDS_NET_LABEL_USED_PORT_FALLBACK),
                    FormatBoolean(host.UsedPortFallback()), BooleanStyle(!host.UsedPortFallback()));
                WriteField(ResourceString(IDS_RTP_LABEL_RECOVERY_JOURNAL),
                    FormatBoolean(host.SendRecoveryJournal()), BooleanStyle(host.SendRecoveryJournal()));

                if (host.NetworkAdapterId() != winrt::guid{})
                {
                    WriteField(ResourceString(IDS_NET_LABEL_ALLOW_ADAPTER_FALLBACK),
                        FormatBoolean(host.AllowNetworkAdapterFallback()), BooleanStyle(host.AllowNetworkAdapterFallback()));
                }
            }

            if (host.UsedPortFallback())
            {
                WriteBlankLine();
                WriteWarningLine(FormatResourceString(IDS_RTP_PORT_FALLBACK_NOTE, WantedPort(host), fmt::format("{}", host.ActualPort())));
            }

            if (host.IsNetworkAdapterMissing())
            {
                auto const adapterName = host.NetworkAdapterName().empty() ?
                    ResourceString(IDS_NET_ADAPTER_UNKNOWN) :
                    ToUtf8(host.NetworkAdapterName());

                WriteBlankLine();
                WriteWarningLine(FormatResourceString(
                    host.HasStarted() ? IDS_NET_ADAPTER_FALLBACK_NOTE : IDS_NET_ADAPTER_WAITING_NOTE,
                    adapterName));
            }

            if (host.Advertise() && host.ServiceInstanceNameWasChanged())
            {
                WriteBlankLine();
                WriteWarningLine(FormatResourceString(IDS_RTP_NAME_CHANGED_NOTE,
                    ToUtf8(host.ServiceInstanceName()), ToUtf8(host.ActualServiceInstanceName())));
            }

            WriteBlankLine();

            WriteHostConnections(host, options.Verbose);
            WriteKnownClients(host);
        }

        return 0;
    }

    int RunRtpClientsCommand(_In_ RtpListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const clients = midi2rtp::MidiRtpTransportManager::GetConfiguredClients();

        if (clients == nullptr || clients.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_RTP_NO_CLIENTS));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_RTP_CLIENTS_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_NET_LABEL_ENTRY_STATE));
        table.AddColumn(ResourceString(IDS_NET_LABEL_REMOTE), ColumnAlignment::Left, fieldValueTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_RTP_LABEL_CONNECTED));
        table.AddColumn(ResourceString(IDS_NET_LABEL_LATENCY), ColumnAlignment::Right, numberTextStyle);
        table.AddColumn(ResourceString(IDS_RTP_LABEL_MIDI_PACKETS), ColumnAlignment::Right, numberTextStyle);

        for (auto const& client : clients)
        {
            if (client == nullptr)
            {
                continue;
            }

            auto const connection = client.Connection();
            bool const connected = connection != nullptr && connection.IsConnected();

            table.BeginRow();
            table.AddCell(ClientDisplayName(client));
            table.AddCell(FormatClientEntryState(client.EntryState()));

            // where it really is once connected, and what it is looking for until then
            table.AddCell(connection != nullptr ?
                FormatAddressAndPort(connection.RemoteAddress(), connection.RemotePort()) :
                ClientTarget(client));

            table.AddCell(FormatBoolean(connected), BooleanStyle(connected));
            table.AddCell(connection != nullptr ? FormatLatency(connection.CurrentLatencyTicks()) : ResourceString(IDS_LABEL_UNKNOWN));
            table.AddCell(connection != nullptr ? FormatPackets(connection) : std::string{ "0 / 0" });

            if (!connected)
            {
                if (auto const problem = DescribeClientProblem(client.LastErrorCode()); !problem.empty())
                {
                    table.AddRowDetail(problem, warningTextStyle);
                }
            }

            if (connection != nullptr && !connection.EndpointDeviceId().empty())
            {
                table.AddRowDetail(ToUtf8(connection.EndpointDeviceId()), endpointIdTextStyle);
            }
        }

        table.Render();

        if (!options.Verbose)
        {
            return 0;
        }

        for (auto const& client : clients)
        {
            if (client == nullptr)
            {
                continue;
            }

            WriteBlankLine();
            WriteSectionHeading(FormatGuid(client.ClientId()));

            WriteField(ResourceString(IDS_NET_LABEL_DIRECT),
                FormatBoolean(client.IsDirectConnection()), BooleanStyle(client.IsDirectConnection()));
            WriteField(ResourceString(IDS_RTP_LABEL_TARGET), ClientTarget(client), fieldValueTextStyle);
            WriteField(ResourceString(IDS_RTP_LABEL_NAME_REMOTE_SEES), ToUtf8(client.Name()), fieldValueTextStyle);
            WriteField(ResourceString(IDS_RTP_LABEL_CUSTOM_ENDPOINT_NAME),
                client.CustomEndpointName().empty() ? ResourceString(IDS_LABEL_NONE) : ToUtf8(client.CustomEndpointName()),
                endpointNameTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_ENABLED),
                FormatBoolean(client.IsEnabled()), BooleanStyle(client.IsEnabled()));
            WriteField(ResourceString(IDS_RTP_LABEL_AUTO_RECONNECT),
                FormatBoolean(client.AutoReconnect()), BooleanStyle(client.AutoReconnect()));
            WriteField(ResourceString(IDS_RTP_LABEL_RECOVERY_JOURNAL),
                FormatBoolean(client.SendRecoveryJournal()), BooleanStyle(client.SendRecoveryJournal()));
            WriteField(ResourceString(IDS_NET_LABEL_SEND_SPEED),
                FormatSendSpeedLimit(static_cast<uint32_t>(client.SendSpeedLimit())), fieldValueTextStyle);

            if (client.LastErrorCode() != 0)
            {
                WriteField(ResourceString(IDS_RTP_LABEL_LAST_ERROR), FormatHresult(client.LastErrorCode()), warningTextStyle);
            }

            if (auto const connection = client.Connection(); connection != nullptr)
            {
                if (!connection.RemoteHostName().empty())
                {
                    WriteField(ResourceString(IDS_NET_LABEL_HOST_NAME), ToUtf8(connection.RemoteHostName()), fieldValueTextStyle);
                }

                WriteField(ResourceString(IDS_RTP_LABEL_LOCAL_PORT), fmt::format("{}", connection.LocalPort()), numberTextStyle);
                WriteField(ResourceString(IDS_RTP_LABEL_BEST_LATENCY), FormatLatency(connection.BestLatencyTicks()), numberTextStyle);
                WriteField(ResourceString(IDS_RTP_LABEL_MESSAGES),
                    fmt::format("{} / {}", connection.TotalCountMessagesReceived(), connection.TotalCountMessagesSent()),
                    numberTextStyle);
                WriteField(ResourceString(IDS_RTP_LABEL_LOSSES),
                    fmt::format("{} / {} / {}",
                        connection.TotalCountPacketsLost(),
                        connection.TotalCountLossesRepairedFromJournal(),
                        connection.TotalCountNoteOffsRecovered()),
                    numberTextStyle);
            }
        }

        return 0;
    }

    int RunRtpBrowseCommand(_In_ RtpListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const hosts = midi2rtp::MidiRtpTransportManager::GetAdvertisedHosts();

        if (hosts == nullptr || hosts.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_RTP_NO_ADVERTISED));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_RTP_ADVERTISED_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_NET_LABEL_SERVICE_INSTANCE), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();

        if (options.Verbose)
        {
            table.AddColumn(ResourceString(IDS_NET_LABEL_HOST_NAME), ColumnAlignment::Left, fieldValueTextStyle);
            table.SetLastColumnShrinkable();
        }

        table.AddColumn(ResourceString(IDS_LABEL_ADDRESS), ColumnAlignment::Left, fieldValueTextStyle);

        for (auto const& host : hosts)
        {
            if (host == nullptr)
            {
                continue;
            }

            auto const addresses = host.IPAddresses();

            // A detail line would turn on separators for every row, so this PC's own host is marked in its name
            table.BeginRow();
            table.AddCell(host.IsThisPc() ?
                FormatResourceString(IDS_RTP_ADVERTISED_BY_THIS_PC_FORMAT, ToUtf8(host.ServiceInstanceName())) :
                ToUtf8(host.ServiceInstanceName()));

            if (options.Verbose)
            {
                table.AddCell(ToUtf8(host.HostName()));
            }

            if (addresses != nullptr && addresses.Size() > 0)
            {
                table.AddCell(FormatAddressAndPort(addresses.GetAt(0), host.Port()));

                // Only the first address goes in the cell proper; the rest stack under it so the
                // IPv6 form lines up with the IPv4 one instead of spanning the table.
                if (options.Verbose)
                {
                    for (uint32_t index = 1; index < addresses.Size(); index++)
                    {
                        table.AddCellLine(FormatAddressAndPort(addresses.GetAt(index), host.Port()));
                    }
                }
            }
            else
            {
                table.AddCell(FormatAddressAndPort(host.HostName(), host.Port()));
            }
        }

        table.Render();

        return 0;
    }

    int RunRtpPendingCommand()
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const pending = midi2rtp::MidiRtpTransportManager::GetPendingRemoteClients();

        if (pending == nullptr || pending.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_NET_NO_PENDING));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_NET_PENDING_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_LABEL_ADDRESS), ColumnAlignment::Left, fieldValueTextStyle);
        table.AddColumn(ResourceString(IDS_NET_LABEL_SERVICE_INSTANCE), ColumnAlignment::Left, fieldValueTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_RTP_LABEL_REQUESTED), ColumnAlignment::Left, timestampTextStyle);

        for (auto const& client : pending)
        {
            if (client == nullptr)
            {
                continue;
            }

            table.BeginRow();
            table.AddCell(ToUtf8(client.RemoteClientName()));
            table.AddCell(ToUtf8(client.RemoteAddress()));
            table.AddCell(ToUtf8(client.HostServiceInstanceName()));
            table.AddCell(FormatDateTime(client.RequestTime()));

            if (client.IsApproved())
            {
                table.AddRowDetail(ResourceString(IDS_RTP_PENDING_APPROVED_NOTE), successTextStyle);
            }
        }

        table.Render();

        return 0;
    }

    int RunRtpStatusCommand(_In_ RtpListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        WriteSectionHeading(ResourceString(IDS_RTP_STATUS_TITLE));

        if (options.Verbose)
        {
            WriteField(ResourceString(IDS_RTP_LABEL_TRANSPORT_ID),
                FormatGuid(midi2rtp::MidiRtpTransportManager::TransportId()), guidTextStyle);
        }

        WriteField(ResourceString(IDS_NET_LABEL_DNS_SERVICE_TYPE),
            ToUtf8(midi2rtp::MidiRtpTransportManager::MidiRtpDnsServiceType()), fieldValueTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_DNS_DOMAIN),
            ToUtf8(midi2rtp::MidiRtpTransportManager::MidiRtpDnsDomain()), fieldValueTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_FULL_SERVICE_NAME),
            ToUtf8(midi2rtp::MidiRtpTransportManager::MidiRtpDnsSdQueryName()), fieldValueTextStyle);
        WriteField(ResourceString(IDS_RTP_LABEL_DEFAULT_PORT),
            fmt::format("{}", midi2rtp::MidiRtpTransportManager::DefaultHostPort()), numberTextStyle);

        auto const hosts = midi2rtp::MidiRtpTransportManager::GetConfiguredHosts();
        auto const clients = midi2rtp::MidiRtpTransportManager::GetConfiguredClients();
        auto const advertised = midi2rtp::MidiRtpTransportManager::GetAdvertisedHosts();
        auto const pending = midi2rtp::MidiRtpTransportManager::GetPendingRemoteClients();

        uint32_t connectedCount{ 0 };

        if (hosts != nullptr)
        {
            for (auto const& host : hosts)
            {
                if (host == nullptr || host.Connections() == nullptr)
                {
                    continue;
                }

                for (auto const& connection : host.Connections())
                {
                    if (connection != nullptr && connection.IsConnected())
                    {
                        connectedCount++;
                    }
                }
            }
        }

        if (clients != nullptr)
        {
            for (auto const& client : clients)
            {
                if (client != nullptr && client.Connection() != nullptr && client.Connection().IsConnected())
                {
                    connectedCount++;
                }
            }
        }

        // an approved remote is no longer waiting on anyone here
        uint32_t awaitingCount{ 0 };

        if (pending != nullptr)
        {
            for (auto const& client : pending)
            {
                if (client != nullptr && !client.IsApproved())
                {
                    awaitingCount++;
                }
            }
        }

        WriteBlankLine();

        WriteInfoLine(FormatResourceString(IDS_RTP_SUMMARY,
            fmt::format("{}", hosts == nullptr ? 0 : hosts.Size()),
            fmt::format("{}", clients == nullptr ? 0 : clients.Size()),
            fmt::format("{}", connectedCount),
            fmt::format("{}", advertised == nullptr ? 0 : advertised.Size()),
            fmt::format("{}", awaitingCount)));

        return 0;
    }
}
