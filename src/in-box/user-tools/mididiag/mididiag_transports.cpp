// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// What the running service says about itself and its transports. Every call here goes to the
// MIDI service, so these sections run only after it has answered once.

#include "pch.h"
#include "mididiag_output.h"
#include "mididiag_sections.h"

#include <winrt/Windows.Devices.Midi2.Transports.Bluetooth.h>
#include <winrt/Windows.Devices.Midi2.Transports.Network.h>
#include <winrt/Windows.Devices.Midi2.Transports.Rtp.h>
#include <winrt/Windows.Devices.Midi2.Transports.Loopback.h>
#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>

using namespace mididiag;

namespace bt = winrt::Windows::Devices::Midi2::Transports::Bluetooth;
namespace net = winrt::Windows::Devices::Midi2::Transports::Network;
namespace rtp = winrt::Windows::Devices::Midi2::Transports::Rtp;
namespace loop = winrt::Windows::Devices::Midi2::Transports::Loopback;
namespace bloop = winrt::Windows::Devices::Midi2::Transports::BasicLoopback;

namespace
{
    // a service that is already up answers in milliseconds; one that has to start first may not
    constexpr std::chrono::seconds RunningServiceResponseTimeout{ 20 };
    constexpr std::chrono::seconds StartingServiceResponseTimeout{ 90 };
    constexpr std::chrono::seconds ServiceStartWait{ 30 };

    // past this, a customer notices the wait every time an app opens a port
    constexpr uint64_t SlowConnectionMilliseconds{ 1000 };

    std::wstring GuidText(_In_ winrt::guid const& id)
    {
        return internal::GuidToString(id);
    }

    uint64_t MillisecondsSince(_In_ std::chrono::steady_clock::time_point const start)
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count());
    }

    std::wstring TimeSpanMilliseconds(_In_ foundation::TimeSpan const span)
    {
        return std::format(L"{:.2f}", std::chrono::duration<double, std::milli>(span).count());
    }

    std::wstring LatencyMilliseconds(_In_ uint64_t const ticks)
    {
        static auto const frequency = midi2::MidiClock::TimestampFrequency();

        if (frequency == 0)
        {
            return {};
        }

        return std::format(L"{:.2f}", static_cast<double>(ticks) * 1000.0 / static_cast<double>(frequency));
    }

    std::wstring BluetoothAddressText(_In_ uint64_t const address)
    {
        return std::format(L"{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
            (address >> 40) & 0xFF, (address >> 32) & 0xFF, (address >> 24) & 0xFF,
            (address >> 16) & 0xFF, (address >> 8) & 0xFF, address & 0xFF);
    }

    // -1 keeps the endpoint while the device is away, -2 defers to the transport setting
    std::wstring OfflineRetentionText(_In_ int32_t const seconds)
    {
        switch (seconds)
        {
        case -1:    return L"always";
        case -2:    return L"default";
        case 0:     return L"immediate";
        default:    return std::to_wstring(seconds);
        }
    }

    std::wstring JoinMaskedAddresses(_In_ collections::IVectorView<winrt::hstring> const& addresses)
    {
        std::wstring joined{};

        if (addresses == nullptr)
        {
            return joined;
        }

        for (auto const& address : addresses)
        {
            joined += joined.empty() ? L"" : L",";
            joined += MaskIpAddress(address);
        }

        return joined;
    }

    // capability names come from the transport, so keep them usable as keys
    std::wstring KeyFromName(_In_ std::wstring_view const name)
    {
        std::wstring key{ name };

        for (auto& ch : key)
        {
            if (!::iswalnum(ch) && ch != L'_')
            {
                ch = L'_';
            }
        }

        return key;
    }

    void AddPendingApprovalFinding(_In_ uint32_t const pendingCount)
    {
        if (pendingCount > 0)
        {
            AddFinding(L"pending_approval", FormatResourceString(IDS_FINDING_PENDING_APPROVAL, pendingCount));
        }
    }

    void AddFirewallFindingIfBlocked(_In_ bool const anyHostRunning)
    {
        auto const& context = Context();

        if (anyHostRunning && context.FirewallStateKnown && context.AnyConnectedNetworkBlocksMidiService)
        {
            AddFinding(L"firewall_blocks_host", internal::ResourceGetWString(IDS_FINDING_FIREWALL_BLOCKS_HOST));
        }
    }
}

bool DoSectionServiceResponse()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_SERVICE_RESPONSE);

    auto const& context = Context();

    SetCurrentSectionTimeout(context.ServiceRunningBeforeReport ?
        RunningServiceResponseTimeout : StartingServiceResponseTimeout);

    auto const start = std::chrono::steady_clock::now();

    // the cheapest call there is, and a stuck service never answers it
    auto const sessions = rept::MidiReporting::GetActiveSessions();

    WriteNumberField(MIDIDIAG_FIELD_LABEL_SERVICE_RESPONSE_MS, MillisecondsSince(start));

    auto state = MidiServiceState();

    while (state == SERVICE_START_PENDING && std::chrono::steady_clock::now() - start < ServiceStartWait)
    {
        ::Sleep(250);
        state = MidiServiceState();
    }

    bool const running = state == SERVICE_RUNNING;

    WriteBoolField(MIDIDIAG_FIELD_LABEL_SERVICE_RUNNING, running);
    WriteBoolField(MIDIDIAG_FIELD_LABEL_SERVICE_STARTED_BY_REPORT, running && !context.ServiceRunningBeforeReport);

    if (!running)
    {
        AddFinding(L"service_not_running", internal::ResourceGetWString(IDS_FINDING_SERVICE_NOT_RUNNING));
        return false;
    }

    return true;
}

void OutputTransportCapabilities(winrt::guid const& transportId, winrt::hstring const& transportCode)
{
    try
    {
        auto const capabilities = svc::MidiServiceTransportPluginConfigManager::QueryAllCapabilities(transportId);

        if (capabilities == nullptr || capabilities.Size() == 0)
        {
            return;
        }

        std::vector<std::pair<std::wstring, bool>> sorted{};

        for (auto const& capability : capabilities)
        {
            sorted.emplace_back(KeyFromName(capability.Key()), capability.Value());
        }

        std::sort(sorted.begin(), sorted.end());

        KeyValueText values{};
        values.Add(L"code", transportCode);

        for (auto const& [name, supported] : sorted)
        {
            values.AddBool(name, supported);
        }

        WriteField(MIDIDIAG_FIELD_LABEL_TRANSPORT_CAPABILITIES, values);
    }
    catch (...)
    {
        // a transport that cannot answer the question simply has nothing to list
    }
}

bool DoSectionBluetooth()
{
    if (!bt::MidiBluetoothTransportManager::IsTransportAvailable())
    {
        return true;
    }

    WriteSection(MIDIDIAG_SECTION_LABEL_BLUETOOTH);

    try
    {
        uint32_t pendingCount{ 0 };

        if (auto const radio = bt::MidiBluetoothTransportManager::GetRadioInformation(); radio != nullptr)
        {
            WriteField(MIDIDIAG_FIELD_LABEL_BLUETOOTH_RADIO, KeyValueText{}
                .AddBool(L"present", radio.IsPresent())
                .AddBool(L"low_energy", radio.IsLowEnergySupported())
                .AddBool(L"central", radio.IsCentralRoleSupported())
                .AddBool(L"peripheral", radio.IsPeripheralRoleSupported()));

            if (!radio.IsPresent() || !radio.IsLowEnergySupported())
            {
                AddFinding(L"bluetooth_no_le_radio", internal::ResourceGetWString(IDS_FINDING_BLUETOOTH_NO_LE_RADIO));
            }
        }

        WriteField(MIDIDIAG_FIELD_LABEL_BLUETOOTH_DEFAULT_RETENTION,
            OfflineRetentionText(bt::MidiBluetoothTransportManager::GetDefaultOfflineRetentionSeconds()));

        for (auto const& device : bt::MidiBluetoothTransportManager::GetAvailableDevices())
        {
            std::wstring state{};

            switch (device.ConnectionState())
            {
            case bt::MidiBluetoothConnectionState::WaitingForDevice:    state = L"waiting_for_device"; break;
            case bt::MidiBluetoothConnectionState::Connecting:          state = L"connecting"; break;
            case bt::MidiBluetoothConnectionState::Connected:           state = L"connected"; break;
            default:                                                    state = L"not_connected"; break;
            }

            std::wstring protocol{};

            switch (device.SelectedProtocol())
            {
            case bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi1:    protocol = L"ble_midi1"; break;
            case bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump: protocol = L"ble_midi2"; break;
            default:                                                    protocol = L"unknown"; break;
            }

            std::wstring timestamps{};

            switch (device.TimestampSource())
            {
            case bt::MidiBluetoothTimestampSource::Device:              timestamps = L"device"; break;
            case bt::MidiBluetoothTimestampSource::ArrivalTime:         timestamps = L"arrival"; break;
            default:                                                    timestamps = L"unknown"; break;
            }

            auto const address = BluetoothAddressText(device.BluetoothAddress());

            KeyValueText values{};
            values.Add(L"address", address)
                .Add(L"state", state)
                .Add(L"protocol", protocol)
                .AddBool(L"paired", device.IsPaired())
                .AddBool(L"requires_pairing", device.RequiresPairing())
                .AddBool(L"present", device.IsPresent());

            if (device.HasBeenSeen())
            {
                values.AddSignedNumber(L"rssi_dbm", device.SignalStrengthDecibelMilliwatts())
                    .AddNumber(L"last_seen_s", static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::seconds>(device.LastSeenAgo()).count()));
            }

            values.AddBool(L"endpoint", device.HasEndpoint())
                .AddNumber(L"messages_in", device.MessagesReceived())
                .AddNumber(L"messages_out", device.MessagesSent())
                .AddNumber(L"packets_in", device.PacketsReceived())
                .AddNumber(L"packets_out", device.PacketsSent())
                .Add(L"timestamps", timestamps)
                .Add(L"interval_ms", TimeSpanMilliseconds(device.ConnectionInterval()))
                .Add(L"offline_retention", OfflineRetentionText(device.EffectiveOfflineRetentionSeconds()))
                .Add(L"name", device.Name());

            WriteField(MIDIDIAG_FIELD_LABEL_BLUETOOTH_DEVICE, values);

            if (!device.LastConnectError().empty() || device.LastConnectErrorHResult() != 0 || device.LastSendErrorHResult() != 0)
            {
                WriteField(MIDIDIAG_FIELD_LABEL_BLUETOOTH_DEVICE_ERROR, KeyValueText{}
                    .Add(L"address", address)
                    .AddNumber(L"connect_code", static_cast<uint32_t>(device.LastConnectErrorCode()))
                    .Add(L"connect_hresult", FormatHResult(device.LastConnectErrorHResult()))
                    .Add(L"send_hresult", FormatHResult(device.LastSendErrorHResult()))
                    .Add(L"text", device.LastConnectError()));
            }

            if (device.RequiresPairing() && !device.IsPaired())
            {
                AddFinding(L"bluetooth_requires_pairing",
                    FormatResourceString(IDS_FINDING_BLUETOOTH_REQUIRES_PAIRING, std::wstring{ device.Name() }));
            }
        }

        for (auto const& saved : bt::MidiBluetoothTransportManager::GetSavedDevices())
        {
            WriteField(MIDIDIAG_FIELD_LABEL_BLUETOOTH_SAVED_DEVICE, KeyValueText{}
                .Add(L"id", saved.BluetoothDeviceId())
                .AddBool(L"enabled", saved.IsEnabled())
                .Add(L"offline_retention", OfflineRetentionText(saved.OfflineRetentionSeconds()))
                .AddIfNotEmpty(L"comment", saved.Comment()));
        }

        if (auto const peripheral = bt::MidiBluetoothTransportManager::GetPeripheralStatus(); peripheral != nullptr)
        {
            KeyValueText values{};
            values.AddBool(L"running", peripheral.IsRunning());

            if (peripheral.IsRunning())
            {
                values.Add(L"protocol", peripheral.Protocol() == bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump ? L"ble_midi2" : L"ble_midi1")
                    .Add(L"policy", peripheral.ClientPolicy() == bt::MidiBluetoothPeripheralClientPolicy::AllowAny ? L"allow_any" : L"require_approval")
                    .AddNumber(L"subscribed_clients", peripheral.SubscribedClientCount())
                    .AddBool(L"client_connected", peripheral.IsClientConnected())
                    .AddNumber(L"messages_in", peripheral.MessagesReceived())
                    .AddNumber(L"messages_out", peripheral.MessagesSent())
                    .AddNumber(L"packets_in", peripheral.PacketsReceived())
                    .AddNumber(L"packets_out", peripheral.PacketsSent())
                    .Add(L"name", peripheral.AdvertisedName());
            }

            WriteField(MIDIDIAG_FIELD_LABEL_BLUETOOTH_PERIPHERAL, values);
        }

        for (auto const& client : bt::MidiBluetoothTransportManager::GetPendingPeripheralClients())
        {
            pendingCount++;

            WriteField(MIDIDIAG_FIELD_LABEL_PENDING_REMOTE_CLIENT, KeyValueText{}
                .Add(L"address", BluetoothAddressText(client.BluetoothAddress()))
                .AddBool(L"paired", client.IsPaired())
                .Add(L"requested", FormatLocalTime(client.ApprovalRequestedTime()))
                .Add(L"name", client.Name()));
        }

        AddPendingApprovalFinding(pendingCount);
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionNetworkMidi2()
{
    if (!net::MidiNetworkTransportManager::IsTransportAvailable())
    {
        return true;
    }

    WriteSection(MIDIDIAG_SECTION_LABEL_NETWORK_MIDI2);

    try
    {
        bool anyHostRunning{ false };
        uint32_t pendingCount{ 0 };

        auto const policyName = [](net::MidiNetworkRemoteClientPolicy const policy)
            {
                return policy == net::MidiNetworkRemoteClientPolicy::RequireApproval ? L"require_approval" : L"allow_any";
            };

        for (auto const& host : net::MidiNetworkTransportManager::GetConfiguredHosts())
        {
            anyHostRunning = anyHostRunning || host.HasStarted();

            WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_HOST, KeyValueText{}
                .Add(L"id", GuidText(host.HostId()))
                .AddBool(L"enabled", host.IsEnabled())
                .AddBool(L"started", host.HasStarted())
                .Add(L"address", MaskIpAddress(host.ActualAddress()))
                .Add(L"port", host.ActualPort())
                .AddBool(L"port_fallback_used", host.UsedPortFallback())
                .Add(L"policy", policyName(host.RemoteClientPolicy()))
                .AddBool(L"midi1_ports", host.CreateMidi1Ports())
                .AddBool(L"adapter_limited", host.NetworkAdapterId() != winrt::guid{})
                .AddBool(L"adapter_missing", host.IsNetworkAdapterMissing())
                .AddBool(L"adapter_fallback_allowed", host.AllowNetworkAdapterFallback())
                .AddNumber(L"connections", host.Connections() == nullptr ? 0 : host.Connections().Size())
                .Add(L"service_instance", host.ActualServiceInstanceName())
                .Add(L"name", host.UmpEndpointName()));

            if (host.Connections() == nullptr)
            {
                continue;
            }

            for (auto const& connection : host.Connections())
            {
                WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_HOST_CONNECTION, KeyValueText{}
                    .Add(L"host", GuidText(host.HostId()))
                    .AddBool(L"session_active", connection.IsSessionActive())
                    .AddBool(L"pending_approval", connection.IsPendingApproval())
                    .Add(L"latency_ms", LatencyMilliseconds(connection.CurrentLatencyTicks()))
                    .AddNumber(L"retransmits", connection.RetransmitCount())
                    .AddNumber(L"retransmit_requests", connection.RetransmitRequestCount())
                    .AddNumber(L"packets_out", connection.TotalCountNetworkPacketsSent())
                    .AddNumber(L"packets_in", connection.TotalCountNetworkPacketsReceived())
                    .Add(L"address", MaskIpAddress(connection.RemoteAddress()))
                    .Add(L"port", connection.RemotePort())
                    .Add(L"name", connection.UmpEndpointName()));
            }
        }

        for (auto const& client : net::MidiNetworkTransportManager::GetConfiguredClients())
        {
            std::wstring state{};

            switch (client.EntryState())
            {
            case net::MidiNetworkClientEntryState::Active:      state = L"active"; break;
            case net::MidiNetworkClientEntryState::Failed:      state = L"failed"; break;
            case net::MidiNetworkClientEntryState::Unavailable: state = L"unavailable"; break;
            default:                                            state = L"pending"; break;
            }

            auto const address = client.ConnectedRemoteAddress().empty() ? client.ConfiguredDirectAddress() : client.ConnectedRemoteAddress();
            auto const port = client.ConnectedRemotePort().empty() ? client.ConfiguredDirectPort() : client.ConnectedRemotePort();

            WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_CLIENT, KeyValueText{}
                .Add(L"id", GuidText(client.ClientId()))
                .Add(L"state", state)
                .AddBool(L"session_active", client.IsSessionActive())
                .AddBool(L"direct", client.IsDirectConnection())
                .Add(L"address", MaskIpAddress(address))
                .Add(L"port", port)
                .Add(L"latency_ms", LatencyMilliseconds(client.CurrentLatencyTicks()))
                .AddNumber(L"retransmits", client.RetransmitCount())
                .AddNumber(L"retransmit_requests", client.RetransmitRequestCount())
                .AddNumber(L"packets_out", client.TotalCountNetworkPacketsSent())
                .AddNumber(L"packets_in", client.TotalCountNetworkPacketsReceived())
                .AddIfNotEmpty(L"match", client.MatchDeviceId()));
        }

        for (auto const& pending : net::MidiNetworkTransportManager::GetPendingRemoteClients())
        {
            pendingCount++;

            WriteField(MIDIDIAG_FIELD_LABEL_PENDING_REMOTE_CLIENT, KeyValueText{}
                .Add(L"host", GuidText(pending.HostId()))
                .Add(L"address", MaskIpAddress(pending.RemoteAddress()))
                .Add(L"requested", FormatLocalTime(pending.RequestTime()))
                .Add(L"name", pending.UmpEndpointName()));
        }

        for (auto const& advertised : net::MidiNetworkTransportManager::GetAdvertisedHosts())
        {
            WriteField(MIDIDIAG_FIELD_LABEL_ADVERTISED_HOST, KeyValueText{}
                .Add(L"service_instance", advertised.ServiceInstanceName())
                .Add(L"host", advertised.HostName())
                .AddNumber(L"port", advertised.Port())
                .Add(L"addresses", JoinMaskedAddresses(advertised.IPAddresses()))
                .Add(L"last_seen", FormatLocalTime(advertised.LastSeenTime()))
                .Add(L"name", advertised.UmpEndpointName()));
        }

        for (auto const& saved : net::MidiNetworkTransportManager::GetSavedHosts())
        {
            WriteField(MIDIDIAG_FIELD_LABEL_SAVED_HOST, KeyValueText{}
                .Add(L"id", GuidText(saved.HostId()))
                .AddBool(L"enabled", saved.IsEnabled())
                .AddBool(L"advertise", saved.Advertise())
                .Add(L"port", saved.UseAutomaticPortAllocation() ? winrt::hstring{ L"auto" } : saved.ManuallyAssignedPort())
                .Add(L"policy", policyName(saved.RemoteClientPolicy()))
                .Add(L"service_instance", saved.ServiceInstanceName())
                .Add(L"name", saved.Name()));
        }

        for (auto const& saved : net::MidiNetworkTransportManager::GetSavedClients())
        {
            KeyValueText values{};
            values.Add(L"id", GuidText(saved.ClientId()))
                .AddBool(L"enabled", saved.IsEnabled());

            if (auto const match = saved.MatchCriteria(); match != nullptr)
            {
                values.AddIfNotEmpty(L"match", match.DeviceId())
                    .AddIfNotEmpty(L"address", MaskIpAddress(match.DirectHostNameOrIPAddress()));

                if (match.DirectPort() != 0)
                {
                    values.AddNumber(L"port", match.DirectPort());
                }
            }

            values.Add(L"name", saved.UmpEndpointName());

            WriteField(MIDIDIAG_FIELD_LABEL_SAVED_CLIENT, values);
        }

        AddFirewallFindingIfBlocked(anyHostRunning);
        AddPendingApprovalFinding(pendingCount);
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionRtpMidi()
{
    if (!rtp::MidiRtpTransportManager::IsTransportAvailable())
    {
        return true;
    }

    WriteSection(MIDIDIAG_SECTION_LABEL_RTP_MIDI);

    try
    {
        bool anyHostRunning{ false };
        uint32_t pendingCount{ 0 };

        auto const policyName = [](rtp::MidiRtpRemoteClientPolicy const policy)
            {
                return policy == rtp::MidiRtpRemoteClientPolicy::RequireApproval ? L"require_approval" : L"allow_any";
            };

        auto const connectionValues = [](rtp::MidiRtpConnection const& connection)
            {
                KeyValueText values{};

                values.AddNumber(L"connection", connection.ConnectionId())
                    .AddBool(L"connected", connection.IsConnected())
                    .AddBool(L"this_pc_invited", connection.ThisPcInvited())
                    .Add(L"latency_ms", LatencyMilliseconds(connection.CurrentLatencyTicks()))
                    .AddNumber(L"packets_out", connection.TotalCountNetworkPacketsSent())
                    .AddNumber(L"packets_in", connection.TotalCountNetworkPacketsReceived())
                    .AddNumber(L"packets_lost", connection.TotalCountPacketsLost())
                    .AddNumber(L"journal_repairs", connection.TotalCountLossesRepairedFromJournal())
                    .AddNumber(L"messages_out", connection.TotalCountMessagesSent())
                    .AddNumber(L"messages_in", connection.TotalCountMessagesReceived())
                    .Add(L"address", MaskIpAddress(connection.RemoteAddress()))
                    .AddNumber(L"port", connection.RemotePort())
                    .Add(L"name", connection.RemoteName());

                return values;
            };

        for (auto const& host : rtp::MidiRtpTransportManager::GetConfiguredHosts())
        {
            anyHostRunning = anyHostRunning || host.HasStarted();

            auto const connections = host.Connections();

            WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_HOST, KeyValueText{}
                .Add(L"id", GuidText(host.HostId()))
                .AddBool(L"enabled", host.IsEnabled())
                .AddBool(L"started", host.HasStarted())
                .AddNumber(L"port", host.ActualPort())
                .AddBool(L"port_fallback_used", host.UsedPortFallback())
                .Add(L"policy", policyName(host.RemoteClientPolicy()))
                .AddSignedNumber(L"last_error", host.LastErrorCode())
                .AddBool(L"adapter_limited", host.NetworkAdapterId() != winrt::guid{})
                .AddBool(L"adapter_missing", host.IsNetworkAdapterMissing())
                .AddBool(L"adapter_fallback_allowed", host.AllowNetworkAdapterFallback())
                .AddNumber(L"connections", connections == nullptr ? 0 : connections.Size())
                .Add(L"service_instance", host.ActualServiceInstanceName())
                .Add(L"name", host.Name()));

            if (connections == nullptr)
            {
                continue;
            }

            for (auto const& connection : connections)
            {
                WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_HOST_CONNECTION, connectionValues(connection));
            }
        }

        for (auto const& client : rtp::MidiRtpTransportManager::GetConfiguredClients())
        {
            std::wstring state{};

            switch (client.EntryState())
            {
            case rtp::MidiRtpClientEntryState::Active:      state = L"active"; break;
            case rtp::MidiRtpClientEntryState::Retrying:    state = L"retrying"; break;
            case rtp::MidiRtpClientEntryState::Unavailable: state = L"unavailable"; break;
            default:                                        state = L"pending"; break;
            }

            KeyValueText values{};
            values.Add(L"id", GuidText(client.ClientId()))
                .Add(L"state", state)
                .AddBool(L"enabled", client.IsEnabled())
                .AddBool(L"direct", client.IsDirectConnection())
                .AddSignedNumber(L"last_error", client.LastErrorCode())
                .AddIfNotEmpty(L"remote_service_instance", client.RemoteServiceInstanceName())
                .AddIfNotEmpty(L"address", MaskIpAddress(client.ConfiguredDirectAddress()))
                .Add(L"name", client.Name());

            WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_CLIENT, values);

            if (auto const connection = client.Connection(); connection != nullptr)
            {
                WriteField(MIDIDIAG_FIELD_LABEL_NETWORK_CLIENT_CONNECTION, connectionValues(connection));
            }
        }

        for (auto const& pending : rtp::MidiRtpTransportManager::GetPendingRemoteClients())
        {
            if (pending.IsApproved())
            {
                continue;
            }

            pendingCount++;

            WriteField(MIDIDIAG_FIELD_LABEL_PENDING_REMOTE_CLIENT, KeyValueText{}
                .Add(L"host", GuidText(pending.HostId()))
                .Add(L"address", MaskIpAddress(pending.RemoteAddress()))
                .Add(L"requested", FormatLocalTime(pending.RequestTime()))
                .Add(L"name", pending.RemoteClientName()));
        }

        for (auto const& advertised : rtp::MidiRtpTransportManager::GetAdvertisedHosts())
        {
            WriteField(MIDIDIAG_FIELD_LABEL_ADVERTISED_HOST, KeyValueText{}
                .Add(L"service_instance", advertised.ServiceInstanceName())
                .Add(L"host", advertised.HostName())
                .AddNumber(L"port", advertised.Port())
                .Add(L"addresses", JoinMaskedAddresses(advertised.IPAddresses()))
                .AddBool(L"this_pc", advertised.IsThisPc()));
        }

        for (auto const& saved : rtp::MidiRtpTransportManager::GetSavedHosts())
        {
            WriteField(MIDIDIAG_FIELD_LABEL_SAVED_HOST, KeyValueText{}
                .Add(L"id", GuidText(saved.HostId()))
                .AddBool(L"enabled", saved.IsEnabled())
                .AddBool(L"advertise", saved.Advertise())
                .Add(L"port", saved.UseAutomaticPortAllocation() ? std::wstring{ L"auto" } : std::to_wstring(saved.ManuallyAssignedPort()))
                .Add(L"policy", policyName(saved.RemoteClientPolicy()))
                .Add(L"service_instance", saved.ServiceInstanceName())
                .Add(L"name", saved.Name()));
        }

        for (auto const& saved : rtp::MidiRtpTransportManager::GetSavedClients())
        {
            KeyValueText values{};
            values.Add(L"id", GuidText(saved.ClientId()))
                .AddBool(L"enabled", saved.IsEnabled());

            if (auto const match = saved.MatchCriteria(); match != nullptr)
            {
                values.AddIfNotEmpty(L"remote_service_instance", match.ServiceInstanceName())
                    .AddIfNotEmpty(L"address", MaskIpAddress(match.DirectHostNameOrIPAddress()));

                if (match.DirectPort() != 0)
                {
                    values.AddNumber(L"port", match.DirectPort());
                }
            }

            values.Add(L"name", saved.Name());

            WriteField(MIDIDIAG_FIELD_LABEL_SAVED_CLIENT, values);
        }

        AddFirewallFindingIfBlocked(anyHostRunning);
        AddPendingApprovalFinding(pendingCount);
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionLoopback()
{
    if (!loop::MidiLoopbackManager::IsTransportAvailable())
    {
        return true;
    }

    WriteSection(MIDIDIAG_SECTION_LABEL_LOOPBACK);

    try
    {
        WriteBoolField(MIDIDIAG_FIELD_LABEL_FEEDBACK_PROTECTION_AVAILABLE, loop::MidiLoopbackManager::IsFeedbackProtectionAvailable());

        for (auto const& entry : loop::MidiLoopbackManager::GetActiveLoopbackEntries())
        {
            auto const nameA = entry.EndpointA() == nullptr ? winrt::hstring{} : entry.EndpointA().Name();
            auto const nameB = entry.EndpointB() == nullptr ? winrt::hstring{} : entry.EndpointB().Name();

            KeyValueText values{};
            values.Add(L"association", GuidText(entry.AssociationId()))
                .AddBool(L"muted", entry.IsMuted())
                .Add(L"feedback_protection", entry.FeedbackProtection() == loop::MidiLoopbackFeedbackProtection::Off ? L"off" : L"mute")
                .AddBool(L"muted_for_feedback", entry.IsMutedForFeedback());

            if (entry.IsMutedForFeedback())
            {
                values.Add(L"feedback_detected", FormatLocalTime(entry.FeedbackDetectedTime()));

                AddFinding(L"loopback_feedback_muted",
                    FormatResourceString(IDS_FINDING_LOOPBACK_FEEDBACK_MUTED, std::wstring{ nameA }));
            }

            values.Add(L"name_a", nameA)
                .Add(L"name_b", nameB);

            WriteField(MIDIDIAG_FIELD_LABEL_LOOPBACK, values);
        }

        for (auto const& saved : loop::MidiLoopbackManager::GetSavedLoopbackEntries())
        {
            auto const nameA = saved.EndpointDefinitionA() == nullptr ? winrt::hstring{} : saved.EndpointDefinitionA().Name();
            auto const nameB = saved.EndpointDefinitionB() == nullptr ? winrt::hstring{} : saved.EndpointDefinitionB().Name();

            WriteField(MIDIDIAG_FIELD_LABEL_SAVED_LOOPBACK, KeyValueText{}
                .Add(L"association", GuidText(saved.AssociationId()))
                .AddBool(L"muted", saved.IsMuted())
                .Add(L"feedback_protection", saved.FeedbackProtection() == loop::MidiLoopbackFeedbackProtection::Off ? L"off" : L"mute")
                .Add(L"name_a", nameA)
                .Add(L"name_b", nameB));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionBasicLoopback()
{
    if (!bloop::MidiBasicLoopbackManager::IsTransportAvailable())
    {
        return true;
    }

    WriteSection(MIDIDIAG_SECTION_LABEL_BASIC_LOOPBACK);

    try
    {
        WriteBoolField(MIDIDIAG_FIELD_LABEL_FEEDBACK_PROTECTION_AVAILABLE, bloop::MidiBasicLoopbackManager::IsFeedbackProtectionAvailable());

        for (auto const& entry : bloop::MidiBasicLoopbackManager::GetActiveLoopbackEntries())
        {
            KeyValueText values{};
            values.Add(L"association", GuidText(entry.AssociationId()))
                .AddBool(L"muted", entry.IsMuted())
                .Add(L"feedback_protection", entry.FeedbackProtection() == bloop::MidiBasicLoopbackFeedbackProtection::Off ? L"off" : L"mute")
                .AddBool(L"muted_for_feedback", entry.IsMutedForFeedback())
                .AddNumber(L"messages", entry.MessageCount());

            if (entry.IsMutedForFeedback())
            {
                values.Add(L"feedback_detected", FormatLocalTime(entry.FeedbackDetectedTime()));

                AddFinding(L"loopback_feedback_muted",
                    FormatResourceString(IDS_FINDING_LOOPBACK_FEEDBACK_MUTED, std::wstring{ entry.Name() }));
            }

            values.Add(L"name", entry.Name());

            WriteField(MIDIDIAG_FIELD_LABEL_LOOPBACK, values);
        }

        for (auto const& saved : bloop::MidiBasicLoopbackManager::GetSavedLoopbackEntries())
        {
            WriteField(MIDIDIAG_FIELD_LABEL_SAVED_LOOPBACK, KeyValueText{}
                .Add(L"association", GuidText(saved.AssociationId()))
                .AddBool(L"muted", saved.IsMuted())
                .Add(L"feedback_protection", saved.FeedbackProtection() == bloop::MidiBasicLoopbackFeedbackProtection::Off ? L"off" : L"mute")
                .Add(L"name", saved.EndpointDefinition() == nullptr ? winrt::hstring{} : saved.EndpointDefinition().Name()));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionEndpointCustomizations()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_ENDPOINT_CUSTOMIZATIONS);

    try
    {
        auto const customizations = svc::MidiServiceTransportPluginConfigManager::GetEndpointCustomizations();

        uint32_t total{ 0 };
        uint32_t orphaned{ 0 };

        if (customizations != nullptr)
        {
            for (auto const& customization : customizations)
            {
                total++;

                // left behind when a device's identity changed, for example a USB device in another port
                if (!customization.IsOrphaned() || !customization.HasUserContent())
                {
                    continue;
                }

                orphaned++;

                KeyValueText values{};
                values.Add(L"transport", GuidText(customization.TransportId()));

                if (auto const match = customization.MatchCriteria(); match != nullptr)
                {
                    values.AddIfNotEmpty(L"device_instance", match.DeviceInstanceId())
                        .AddIfNotEmpty(L"endpoint", match.EndpointDeviceId());

                    if (match.UsbVendorId() != 0 || match.UsbProductId() != 0)
                    {
                        values.AddHex(L"usb_vid", match.UsbVendorId(), 4)
                            .AddHex(L"usb_pid", match.UsbProductId(), 4);
                    }

                    values.AddIfNotEmpty(L"usb_serial", match.UsbSerialNumber())
                        .AddIfNotEmpty(L"product_instance_id", match.Midi2ProductInstanceId())
                        .AddIfNotEmpty(L"address", MaskIpAddress(match.StaticIPAddress()))
                        .AddIfNotEmpty(L"transport_name", match.TransportSuppliedEndpointName());
                }

                values.AddIfNotEmpty(L"name", customization.Name());

                WriteField(MIDIDIAG_FIELD_LABEL_ORPHANED_CUSTOMIZATION, values);
            }
        }

        WriteNumberField(MIDIDIAG_FIELD_LABEL_CUSTOMIZATION_COUNT, total);
        WriteNumberField(MIDIDIAG_FIELD_LABEL_ORPHANED_CUSTOMIZATION_COUNT, orphaned);

        if (orphaned > 0)
        {
            AddFinding(L"orphaned_customizations", FormatResourceString(IDS_FINDING_ORPHANED_CUSTOMIZATIONS, orphaned));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_EXCEPTION_COLLECTING_SECTION));
        return false;
    }

    return true;
}

bool DoSectionConnectionTiming()
{
    WriteSection(MIDIDIAG_SECTION_LABEL_CONNECTION_TIMING);

    try
    {
        // The diagnostics loopback has no hardware behind it, so this measures only what the
        // service adds to every open: checking the components, building the pipes.
        auto const sessionStart = std::chrono::steady_clock::now();
        auto session = midi2::MidiSession::Create(L"mididiag connection timing");
        WriteNumberField(MIDIDIAG_FIELD_LABEL_SESSION_CREATE_MS, MillisecondsSince(sessionStart));

        if (session == nullptr)
        {
            WriteError(internal::ResourceGetWString(IDS_ERROR_CONNECTION_TEST_FAILED));
            return false;
        }

        auto const closeSession = wil::scope_exit([&session]() noexcept
            {
                try
                {
                    session.Close();
                }
                catch (...)
                {
                }
            });

        auto const openStart = std::chrono::steady_clock::now();
        auto const connection = session.CreateEndpointConnection(diag::MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
        bool const opened = connection != nullptr && connection.Open();
        auto const openMilliseconds = MillisecondsSince(openStart);

        WriteNumberField(MIDIDIAG_FIELD_LABEL_LOOPBACK_OPEN_MS, openMilliseconds);
        WriteBoolField(MIDIDIAG_FIELD_LABEL_LOOPBACK_OPENED, opened);

        if (!opened)
        {
            WriteError(internal::ResourceGetWString(IDS_ERROR_CONNECTION_TEST_FAILED));
            return false;
        }

        if (openMilliseconds > SlowConnectionMilliseconds)
        {
            AddFinding(L"slow_connection", FormatResourceString(IDS_FINDING_SLOW_CONNECTION, openMilliseconds));
        }
    }
    catch (...)
    {
        WriteError(internal::ResourceGetWString(IDS_ERROR_CONNECTION_TEST_FAILED));
        return false;
    }

    return true;
}
