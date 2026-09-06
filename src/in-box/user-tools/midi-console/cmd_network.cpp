// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "cmd_network.h"
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
            if (midi2net::MidiNetworkTransportManager::IsTransportAvailable())
            {
                return true;
            }

            WriteErrorLine(ResourceString(IDS_NET_NOT_AVAILABLE));

            return false;
        }

        std::string FormatRemoteClientPolicy(_In_ midi2net::MidiNetworkRemoteClientPolicy policy)
        {
            switch (policy)
            {
            case midi2net::MidiNetworkRemoteClientPolicy::AllowAny:        return "Allow any";
            case midi2net::MidiNetworkRemoteClientPolicy::RequireApproval: return "Require approval";
            default:                                                       return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }

        std::string FormatClientEntryState(_In_ midi2net::MidiNetworkClientEntryState state)
        {
            switch (state)
            {
            case midi2net::MidiNetworkClientEntryState::Pending:     return "Pending";
            case midi2net::MidiNetworkClientEntryState::Active:      return "Active";
            case midi2net::MidiNetworkClientEntryState::Failed:      return "Failed";
            case midi2net::MidiNetworkClientEntryState::Unavailable: return "Unavailable";
            default:                                                 return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }

        std::string FormatEndpointOrEmpty(_In_ winrt::hstring const& value)
        {
            auto const text = ToUtf8(value);

            return text.empty() ? ResourceString(IDS_LABEL_NONE) : text;
        }

        // Latency is reported in MIDI clock ticks, which mean nothing without the frequency.
        std::string FormatLatency(_In_ uint64_t ticks)
        {
            if (ticks == 0)
            {
                return ResourceString(IDS_LABEL_UNKNOWN);
            }

            return fmt::format("{:.3f} ms", midi2::MidiClock::ConvertTimestampTicksToMilliseconds(ticks));
        }

        std::string FormatAddressAndPort(_In_ winrt::hstring const& address, _In_ winrt::hstring const& port)
        {
            auto const addressText = ToUtf8(address);
            auto const portText = ToUtf8(port);

            if (addressText.empty())
            {
                return portText.empty() ? ResourceString(IDS_LABEL_NONE) : fmt::format(":{}", portText);
            }

            // An IPv6 address is full of colons, so without brackets the port is unreadable.
            if (addressText.find(':') != std::string::npos)
            {
                return fmt::format("[{}]:{}", addressText, portText);
            }

            return fmt::format("{}:{}", addressText, portText);
        }

        // An advertised host address already comes back bare, so this only has to bracket the
        // IPv6 form to keep the port readable.
        std::string FormatHostAddress(_In_ winrt::hstring const& address, _In_ uint16_t port)
        {
            auto const addressText = ToUtf8(address);

            if (addressText.find(':') != std::string::npos)
            {
                return fmt::format("[{}]:{}", addressText, port);
            }

            return fmt::format("{}:{}", addressText, port);
        }

        void WriteHostConnections(_In_ midi2net::MidiNetworkConfiguredHost const& host)
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
            table.AddColumn(ResourceString(IDS_NET_LABEL_SESSION_ACTIVE));
            table.AddColumn(ResourceString(IDS_NET_LABEL_LATENCY), ColumnAlignment::Right, numberTextStyle);
            table.AddColumn(ResourceString(IDS_NET_LABEL_PACKETS), ColumnAlignment::Right, numberTextStyle);

            for (auto const& connection : connections)
            {
                table.BeginRow();
                table.AddCell(ToUtf8(connection.UmpEndpointName()));
                table.AddCell(FormatAddressAndPort(connection.RemoteAddress(), connection.RemotePort()));
                table.AddCell(FormatBoolean(connection.IsSessionActive()), BooleanStyle(connection.IsSessionActive()));
                table.AddCell(FormatLatency(connection.CurrentLatencyTicks()));
                table.AddCell(fmt::format("{} / {}",
                    connection.TotalCountNetworkPacketsReceived(), connection.TotalCountNetworkPacketsSent()));

                if (connection.IsPendingApproval())
                {
                    table.AddRowDetail(ResourceString(IDS_NET_LABEL_PENDING_APPROVAL), warningTextStyle);
                }

                if (!connection.EndpointDeviceId().empty())
                {
                    table.AddRowDetail(ToUtf8(connection.EndpointDeviceId()), endpointIdTextStyle);
                }
            }

            table.Render();
        }
    }

    int RunNetworkHostsCommand(_In_ NetworkListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const hosts = midi2net::MidiNetworkTransportManager::GetConfiguredHosts();

        if (hosts == nullptr || hosts.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_NET_NO_HOSTS));
            return 0;
        }

        for (auto const& host : hosts)
        {
            WriteSectionHeading(ToUtf8(host.UmpEndpointName()));

            WriteField(ResourceString(IDS_NET_LABEL_HOST_ID), FormatGuid(host.HostId()), guidTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_ENABLED),
                FormatBoolean(host.IsEnabled()), BooleanStyle(host.IsEnabled()));
            WriteField(ResourceString(IDS_NET_LABEL_STARTED),
                FormatBoolean(host.HasStarted()), BooleanStyle(host.HasStarted()));
            WriteField(ResourceString(IDS_NET_LABEL_PORT),
                FormatAddressAndPort(host.ActualAddress(), host.ActualPort()), fieldValueTextStyle);

            if (host.UsedPortFallback())
            {
                WriteField(ResourceString(IDS_NET_LABEL_CONFIGURED_PORT),
                    ToUtf8(host.ConfiguredPort()), warningTextStyle);
            }

            WriteField(ResourceString(IDS_NET_LABEL_SERVICE_INSTANCE),
                ToUtf8(host.ActualServiceInstanceName()), fieldValueTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_PRODUCT_INSTANCE_ID),
                ToUtf8(host.ProductInstanceId()), fieldValueTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_REMOTE_POLICY),
                FormatRemoteClientPolicy(host.RemoteClientPolicy()), fieldValueTextStyle);

            if (options.Verbose)
            {
                WriteField(ResourceString(IDS_NET_LABEL_CREATE_MIDI1_PORTS),
                    FormatBoolean(host.CreateMidi1Ports()), BooleanStyle(host.CreateMidi1Ports()));
                WriteField(ResourceString(IDS_NET_LABEL_USED_PORT_FALLBACK),
                    FormatBoolean(host.UsedPortFallback()), BooleanStyle(!host.UsedPortFallback()));
            }

            if (host.UsedPortFallback())
            {
                WriteBlankLine();
                WriteWarningLine(ResourceString(IDS_NET_PORT_FALLBACK_NOTE));
            }

            WriteBlankLine();

            WriteHostConnections(host);
        }

        return 0;
    }

    int RunNetworkClientsCommand(_In_ NetworkListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const clients = midi2net::MidiNetworkTransportManager::GetConfiguredClients();

        if (clients == nullptr || clients.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_NET_NO_CLIENTS));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_NET_CLIENTS_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_NET_LABEL_ENTRY_STATE));
        table.AddColumn(ResourceString(IDS_NET_LABEL_REMOTE), ColumnAlignment::Left, fieldValueTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_NET_LABEL_SESSION_ACTIVE));
        table.AddColumn(ResourceString(IDS_NET_LABEL_LATENCY), ColumnAlignment::Right, numberTextStyle);
        table.AddColumn(ResourceString(IDS_NET_LABEL_PACKETS), ColumnAlignment::Right, numberTextStyle);

        for (auto const& client : clients)
        {
            table.BeginRow();
            table.AddCell(FormatClientEntryState(client.EntryState()));
            table.AddCell(FormatAddressAndPort(client.ConnectedRemoteAddress(), client.ConnectedRemotePort()));
            table.AddCell(FormatBoolean(client.IsSessionActive()), BooleanStyle(client.IsSessionActive()));
            table.AddCell(FormatLatency(client.CurrentLatencyTicks()));
            table.AddCell(fmt::format("{} / {}",
                client.TotalCountNetworkPacketsReceived(), client.TotalCountNetworkPacketsSent()));

            if (!client.EndpointDeviceId().empty())
            {
                table.AddRowDetail(ToUtf8(client.EndpointDeviceId()), endpointIdTextStyle);
            }
        }

        table.Render();

        if (!options.Verbose)
        {
            return 0;
        }

        for (auto const& client : clients)
        {
            WriteBlankLine();
            WriteSectionHeading(FormatGuid(client.ClientId()));

            WriteField(ResourceString(IDS_NET_LABEL_DIRECT),
                FormatBoolean(client.IsDirectConnection()), BooleanStyle(client.IsDirectConnection()));

            if (client.IsDirectConnection())
            {
                WriteField(ResourceString(IDS_NET_LABEL_CONFIGURED_PORT),
                    FormatAddressAndPort(client.ConfiguredDirectAddress(), client.ConfiguredDirectPort()),
                    fieldValueTextStyle);
            }
            else
            {
                WriteField(ResourceString(IDS_NET_LABEL_MATCH_ID),
                    FormatEndpointOrEmpty(client.MatchDeviceId()), fieldValueTextStyle);
            }

            WriteField(ResourceString(IDS_NET_LABEL_LOCAL),
                FormatAddressAndPort(client.ConnectedLocalAddress(), client.ConnectedLocalPort()), fieldValueTextStyle);
            WriteField(ResourceString(IDS_NET_LABEL_RETRANSMITS),
                fmt::format("{} / {}", client.RetransmitCount(), client.RetransmitRequestCount()), numberTextStyle);
        }

        return 0;
    }

    int RunNetworkBrowseCommand(_In_ NetworkListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const hosts = midi2net::MidiNetworkTransportManager::GetAdvertisedHosts();

        if (hosts == nullptr || hosts.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_NET_NO_ADVERTISED));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_NET_ADVERTISED_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_NET_LABEL_SERVICE_INSTANCE), ColumnAlignment::Left, fieldValueTextStyle);
        table.SetLastColumnShrinkable();

        if (options.Verbose)
        {
            table.AddColumn(ResourceString(IDS_NET_LABEL_HOST_NAME), ColumnAlignment::Left, fieldValueTextStyle);
            table.SetLastColumnShrinkable();
        }

        table.AddColumn(ResourceString(IDS_LABEL_ADDRESS), ColumnAlignment::Left, fieldValueTextStyle);

        for (auto const& host : hosts)
        {
            auto const addresses = host.IPAddresses();

            table.BeginRow();
            table.AddCell(ToUtf8(host.UmpEndpointName().empty() ? host.DeviceName() : host.UmpEndpointName()));
            table.AddCell(ToUtf8(host.ServiceInstanceName()));

            if (options.Verbose)
            {
                table.AddCell(ToUtf8(host.HostName()));
            }

            if (addresses != nullptr && addresses.Size() > 0)
            {
                table.AddCell(FormatHostAddress(addresses.GetAt(0), host.Port()));

                // Only the first address goes in the cell proper; the rest stack under it so the
                // IPv6 form lines up with the IPv4 one instead of spanning the table.
                if (options.Verbose)
                {
                    for (uint32_t index = 1; index < addresses.Size(); index++)
                    {
                        table.AddCellLine(FormatHostAddress(addresses.GetAt(index), host.Port()));
                    }
                }
            }
            else
            {
                table.AddCell(FormatHostAddress(host.HostName(), host.Port()));
            }

            if (!options.Verbose)
            {
                continue;
            }

            // Everything else this transport advertises, so the command covers what midimdnsinfo
            // reports. These are long, so they go under the row rather than in columns. Detail
            // text has to stay unstyled: the table measures it as plain text.
            auto detailLine = [](_In_ UINT labelId, _In_ std::string const& value)
                {
                    return fmt::format("{}  {}", PadRightToWidth(ResourceString(labelId), 22), value);
                };

            table.AddRowDetail(detailLine(IDS_LABEL_ID, ToUtf8(host.DeviceId())), endpointIdTextStyle);

            // The advertised device name and the UMP endpoint name are separate fields. Only the
            // one shown in the Name column is guaranteed, so surface the other when they differ.
            if (!host.UmpEndpointName().empty() &&
                !host.DeviceName().empty() &&
                host.UmpEndpointName() != host.DeviceName())
            {
                table.AddRowDetail(detailLine(IDS_NET_LABEL_DEVICE_NAME, ToUtf8(host.DeviceName())),
                    endpointNameTextStyle);
            }

            table.AddRowDetail(detailLine(IDS_NET_LABEL_FULL_SERVICE_NAME, ToUtf8(host.FullName())),
                fieldValueTextStyle);

            table.AddRowDetail(detailLine(IDS_NET_LABEL_DNS_SERVICE_TYPE, ToUtf8(host.ServiceType())),
                fieldValueTextStyle);

            table.AddRowDetail(detailLine(IDS_NET_LABEL_DNS_DOMAIN, ToUtf8(host.Domain())),
                fieldValueTextStyle);

            table.AddRowDetail(detailLine(IDS_NET_LABEL_PRODUCT_INSTANCE_ID, ToUtf8(host.ProductInstanceId())),
                fieldValueTextStyle);
        }

        table.Render();

        return 0;
    }

    int RunNetworkPendingCommand()
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const pending = midi2net::MidiNetworkTransportManager::GetPendingRemoteClients();

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

        for (auto const& client : pending)
        {
            table.BeginRow();
            table.AddCell(ToUtf8(client.UmpEndpointName()));
            table.AddCell(ToUtf8(client.RemoteAddress()));
            table.AddCell(ToUtf8(client.HostServiceInstanceName()));
        }

        table.Render();

        return 0;
    }

    int RunNetworkStatusCommand(_In_ NetworkListOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        WriteSectionHeading(ResourceString(IDS_NET_STATUS_TITLE));

        WriteField(ResourceString(IDS_NET_LABEL_DNS_SERVICE_TYPE),
            ToUtf8(midi2net::MidiNetworkTransportManager::MidiNetworkUdpDnsServiceType()), fieldValueTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_DNS_DOMAIN),
            ToUtf8(midi2net::MidiNetworkTransportManager::MidiNetworkUdpDnsDomain()), fieldValueTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_FULL_SERVICE_NAME),
            ToUtf8(midi2net::MidiNetworkTransportManager::MidiNetworkUdpDnsSdQueryName()), fieldValueTextStyle);

        auto const hosts = midi2net::MidiNetworkTransportManager::GetConfiguredHosts();
        auto const clients = midi2net::MidiNetworkTransportManager::GetConfiguredClients();
        auto const advertised = midi2net::MidiNetworkTransportManager::GetAdvertisedHosts();
        auto const pending = midi2net::MidiNetworkTransportManager::GetPendingRemoteClients();

        WriteBlankLine();

        WriteInfoLine(FormatResourceString(IDS_NET_SUMMARY,
            fmt::format("{}", hosts == nullptr ? 0 : hosts.Size()),
            fmt::format("{}", clients == nullptr ? 0 : clients.Size()),
            fmt::format("{}", advertised == nullptr ? 0 : advertised.Size()),
            fmt::format("{}", pending == nullptr ? 0 : pending.Size())));

        if (!options.Verbose)
        {
            return 0;
        }

        auto const settings = midi2net::MidiNetworkTransportManager::GetTransportSettings();

        if (settings == nullptr)
        {
            return 0;
        }

        WriteBlankLine();
        WriteSectionHeading(ResourceString(IDS_NET_SETTINGS_TITLE));

        WriteField(ResourceString(IDS_NET_LABEL_MAX_FEC),
            fmt::format("{}", settings.MaxForwardErrorCorrectionCommandPackets()), numberTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_MAX_RETRANSMIT_BUFFER),
            fmt::format("{}", settings.MaxRetransmitBufferCommandPackets()), numberTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_PING_INTERVAL),
            fmt::format("{} ms", settings.OutboundPingIntervalMilliseconds()), numberTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_INVITATION_TIMEOUT),
            fmt::format("{} ms", settings.InvitationPendingTimeoutMilliseconds()), numberTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_MAX_HOST_CONNECTIONS),
            fmt::format("{}", settings.MaxHostConnections()), numberTextStyle);
        WriteField(ResourceString(IDS_NET_LABEL_SCAN_INTERVAL),
            fmt::format("{} ms", settings.DirectConnectionScanIntervalMilliseconds()), numberTextStyle);

        return 0;
    }
}
