// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "cmd_bluetooth.h"
#include "console_output.h"
#include "console_table.h"
#include "midi_formatting.h"
#include "return_codes.h"
#include "strings.h"

namespace midi2console
{
    namespace
    {
        constexpr int32_t OfflineRetentionKeepAlways = -1;
        constexpr int32_t OfflineRetentionUseTransportDefault = -2;

        bool TryParseOfflineRetention(_In_ std::string const& text, _Out_ int32_t& seconds)
        {
            seconds = OfflineRetentionUseTransportDefault;

            if (EqualsIgnoreCase(text, "always"))    { seconds = OfflineRetentionKeepAlways; return true; }
            if (EqualsIgnoreCase(text, "default"))   { seconds = OfflineRetentionUseTransportDefault; return true; }
            if (EqualsIgnoreCase(text, "immediate")) { seconds = 0; return true; }

            try
            {
                auto const parsed = std::stol(text);

                if (parsed >= 0 && parsed <= 86400)
                {
                    seconds = static_cast<int32_t>(parsed);
                    return true;
                }
            }
            catch (...)
            {
            }

            WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                text, std::string{ "--keep-when-offline" }));

            return false;
        }

        // Every config write goes through here so "--temporary" consistently means
        // "push to the running service without writing the configuration file".
        bool ApplyConfig(
            _In_ midi2config::IMidiServiceTransportPluginConfig const& config,
            _In_ bool temporary)
        {
            if (temporary)
            {
                auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

                return response != nullptr &&
                    response.Status() == midi2config::MidiServiceConfigResponseStatus::Success;
            }

            auto const response = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

            return response != nullptr && response.Success();
        }

        // Applying a change and remembering it are separate steps. The transport manager call has
        // already taken effect by the time this runs; this only decides whether it survives a
        // service restart.
        void ReportSave(_In_ midi2config::IMidiServiceTransportPluginConfig const& config)
        {
            auto const response = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

            if (response != nullptr && response.Success())
            {
                WriteSuccessLine(ResourceString(IDS_BT_SAVED_TO_CONFIG));

                auto const backup = ToUtf8(response.BackupFilePath());

                if (!backup.empty())
                {
                    WriteInfoLine(FormatResourceString(IDS_CUSTOMIZE_BACKUP_WRITTEN, backup));
                }

                return;
            }

            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteWarningLine(FormatResourceString(IDS_BT_SAVE_FAILED, message));
        }

        bool EnsureTransportAvailable()
        {
            if (midi2bt::MidiBluetoothTransportManager::IsTransportAvailable())
            {
                return true;
            }

            WriteErrorLine(ResourceString(IDS_BT_NOT_AVAILABLE));

            return false;
        }

        // A machine with no radio, or one which cannot act as a central, otherwise looks exactly
        // like a machine where nothing happens to be switched on.
        void ReportRadioLimitations()
        {
            auto const radio = midi2bt::MidiBluetoothTransportManager::GetRadioInformation();

            if (radio == nullptr)
            {
                return;
            }

            if (!radio.IsPresent())
            {
                WriteWarningLine(ResourceString(IDS_BT_RADIO_UNAVAILABLE));
                WriteBlankLine();
            }
            else if (!radio.IsLowEnergySupported() || !radio.IsCentralRoleSupported())
            {
                WriteWarningLine(ResourceString(IDS_BT_RADIO_OFF));
                WriteBlankLine();
            }
        }

        std::string FormatOfflineRetention(_In_ int32_t seconds)
        {
            if (seconds == OfflineRetentionKeepAlways) return ResourceString(IDS_BT_RETENTION_ALWAYS);
            if (seconds <= 0)                          return ResourceString(IDS_BT_RETENTION_IMMEDIATE);

            return FormatResourceString(IDS_BT_RETENTION_SECONDS, fmt::format("{}", seconds));
        }

        // The service holds the whole allow and deny list, so it is read back and written out
        // together rather than merged entry by entry.
        void PersistClientLists(_In_ midi2bt::MidiBluetoothApprovalScope scope)
        {
            if (scope != midi2bt::MidiBluetoothApprovalScope::Always)
            {
                return;
            }

            auto const status = midi2bt::MidiBluetoothTransportManager::GetPeripheralStatus();

            if (status == nullptr)
            {
                WriteWarningLine(FormatResourceString(IDS_BT_SAVE_FAILED, std::string{}));
                return;
            }

            midi2bt::MidiBluetoothPeripheralClientListConfig const config{ status };

            ReportSave(config);
        }

        std::string FormatBluetoothProtocol(_In_ midi2bt::MidiBluetoothProtocol protocol)
        {
            switch (protocol)
            {
            case midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi1:    return "BLE MIDI 1.0";
            case midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump: return "BLE MIDI 2.0";
            default:                                                          return ResourceString(IDS_LABEL_UNKNOWN);
            }
        }

        bool TryParseApprovalScope(_In_ std::string const& text, _Out_ midi2bt::MidiBluetoothApprovalScope& scope)
        {
            scope = midi2bt::MidiBluetoothApprovalScope::Once;

            if (text.empty() || EqualsIgnoreCase(text, "once"))
            {
                return true;
            }

            if (EqualsIgnoreCase(text, "untilrestart"))
            {
                scope = midi2bt::MidiBluetoothApprovalScope::UntilRestart;
                return true;
            }

            if (EqualsIgnoreCase(text, "always"))
            {
                scope = midi2bt::MidiBluetoothApprovalScope::Always;
                return true;
            }

            WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE, text, std::string{ "--scope" }));

            return false;
        }

        // Addresses are shown and accepted as twelve hex digits, not as the raw 64-bit value.
        std::string FormatBluetoothAddress(_In_ uint64_t address)
        {
            return fmt::format("{:012X}", address & 0x0000FFFFFFFFFFFFull);
        }

        void WriteRememberedClients(
            _In_ std::string_view heading,
            _In_ collections::IVectorView<midi2bt::MidiBluetoothRememberedClient> const& clients)
        {
            if (clients == nullptr || clients.Size() == 0)
            {
                return;
            }

            WriteBlankLine();
            WriteLine(fmt::format("  {}", Styled(heading, infoTextStyle)));

            for (auto const& client : clients)
            {
                WriteField("    " + ToUtf8(client.BluetoothAddress()), ToUtf8(client.Name()), endpointNameTextStyle);
            }
        }
    }

    int RunBluetoothListCommand()
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        ReportRadioLimitations();

        auto const devices = midi2bt::MidiBluetoothTransportManager::GetAvailableDevices();

        if (devices == nullptr || devices.Size() == 0)
        {
            WriteWarningLine(ResourceString(IDS_BT_NO_DEVICES));
            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_BT_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_BT_LABEL_DEVICE_ID), ColumnAlignment::Left, endpointIdTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_BT_LABEL_CONNECTED));
        table.AddColumn(ResourceString(IDS_LABEL_PROTOCOL));
        table.AddColumn(ResourceString(IDS_BT_LABEL_IN_COUNTS), ColumnAlignment::Right);
        table.AddColumn(ResourceString(IDS_BT_LABEL_OUT_COUNTS), ColumnAlignment::Right);

        for (auto const& device : devices)
        {
            table.BeginRow();
            table.AddCell(ToUtf8(device.BluetoothDeviceId()));
            table.AddCell(ToUtf8(device.Name()));
            table.AddCell(FormatBoolean(device.IsConnected()), BooleanStyle(device.IsConnected()));
            table.AddCell(FormatBluetoothProtocol(device.SelectedProtocol()));
            table.AddCell(fmt::format("{} / {}", device.MessagesReceived(), device.PacketsReceived()), numberTextStyle);
            table.AddCell(fmt::format("{} / {}", device.MessagesSent(), device.PacketsSent()), numberTextStyle);
        }

        table.Render();

        // Nothing in a Bluetooth advertisement declares this, so it is only ever known after an
        // attempt has been made, and the transport stops retrying once it is.
        for (auto const& device : devices)
        {
            if (device.RequiresPairing())
            {
                auto const name = device.Name().empty() ?
                    ToUtf8(device.BluetoothDeviceId()) : ToUtf8(device.Name());

                WriteBlankLine();
                WriteWarningLine(FormatResourceString(IDS_BT_REQUIRES_PAIRING, name));
            }
        }

        return 0;
    }

    int RunBluetoothConnectCommand(_In_ BluetoothDeviceOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        midi2bt::MidiBluetoothDeviceConnectConfig const config{ winrt::hstring{ FromUtf8(options.BluetoothDeviceId) } };

        WriteInfoLine(ResourceString(IDS_BT_CONNECTING));

        auto const response = midi2bt::MidiBluetoothTransportManager::ConnectDeviceAsync(config).get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_CONNECT_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_CONNECTED));
        WriteBlankLine();

        if (options.Temporary)
        {
            WriteInfoLine(ResourceString(IDS_BT_TEMPORARY_NOTE));

            return 0;
        }

        // A bare Bluetooth address in the configuration file says nothing about the device.
        if (response.Device() != nullptr && !response.Device().Name().empty())
        {
            config.Comment(response.Device().Name());
        }

        ReportSave(config);

        return 0;
    }

    int RunBluetoothDisconnectCommand(_In_ BluetoothDeviceOptions const& options)
    {
        midi2bt::MidiBluetoothDeviceDisconnectConfig const config{
            winrt::hstring{ FromUtf8(options.BluetoothDeviceId) }, options.Forget };

        WriteInfoLine(ResourceString(IDS_BT_DISCONNECTING));

        auto const response = midi2bt::MidiBluetoothTransportManager::DisconnectDeviceAsync(config).get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_DISCONNECT_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_DISCONNECTED));

        return 0;
    }

    int RunBluetoothCustomizeCommand(_In_ BluetoothCustomizeOptions const& options)
    {
        auto const device = midi2bt::MidiBluetoothTransportManager::GetDevice(
            winrt::hstring{ FromUtf8(options.BluetoothDeviceId) });

        if (device == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_BT_NO_DEVICES));
            return 1;
        }

        bool changed{ false };

        if (!options.KeepWhenOffline.empty())
        {
            int32_t retentionSeconds{ 0 };

            if (!TryParseOfflineRetention(options.KeepWhenOffline, retentionSeconds))
            {
                return 1;
            }

            midi2bt::MidiBluetoothOfflineRetentionConfig const retention{
                winrt::hstring{ FromUtf8(options.BluetoothDeviceId) }, retentionSeconds };

            if (!ApplyConfig(retention, options.Temporary))
            {
                WriteErrorLine(FormatResourceString(IDS_BT_CUSTOMIZE_FAILED, std::string{}));
                return 1;
            }

            changed = true;
        }

        if (options.HasName || options.HasDescription || options.HasImage || options.Clear)
        {
            auto const endpointDeviceId = ToUtf8(device.EndpointDeviceId());

            if (endpointDeviceId.empty())
            {
                WriteErrorLine(ResourceString(IDS_ERROR_ENDPOINT_NOT_FOUND));
                return 1;
            }

            midi2config::MidiServiceEndpointCustomizationConfig customization;

            customization.MatchCriteria().EndpointDeviceId(winrt::hstring{ FromUtf8(endpointDeviceId) });

            if (options.Clear)
            {
                customization.ClearDisplayProperties(true);
            }
            else
            {
                if (options.HasName)        customization.Name(winrt::hstring{ FromUtf8(options.Name) });
                if (options.HasDescription) customization.Description(winrt::hstring{ FromUtf8(options.Description) });
                if (options.HasImage)       customization.ImageFileName(winrt::hstring{ FromUtf8(options.Image) });
            }

            if (!ApplyConfig(customization, options.Temporary))
            {
                WriteErrorLine(FormatResourceString(IDS_BT_CUSTOMIZE_FAILED, std::string{}));
                return 1;
            }

            changed = true;
        }

        if (!changed)
        {
            WriteWarningLine(ResourceString(IDS_ERROR_NO_REQUEST_FLAGS));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_CUSTOMIZE_SUCCEEDED));

        return 0;
    }

    int RunBluetoothPeripheralStartCommand(_In_ BluetoothPeripheralStartOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        midi2bt::MidiBluetoothPeripheralConfig config;

        auto protocol = midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi1;

        if (!options.Protocol.empty())
        {
            if (EqualsIgnoreCase(options.Protocol, "midi2"))
            {
                protocol = midi2bt::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump;
            }
            else if (!EqualsIgnoreCase(options.Protocol, "midi1"))
            {
                WriteErrorLine(FormatResourceString(IDS_ERROR_INVALID_ENUM_VALUE,
                    options.Protocol, std::string{ "--protocol" }));
                return 1;
            }
        }

        config.Protocol(protocol);
        config.IsEnabled(true);

        auto const response = midi2bt::MidiBluetoothTransportManager::StartPeripheralAsync(config).get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_PERIPHERAL_START_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_PERIPHERAL_STARTED));
        WriteBlankLine();

        if (options.Temporary)
        {
            WriteInfoLine(ResourceString(IDS_BT_TEMPORARY_NOTE));

            return 0;
        }

        ReportSave(config);

        return 0;
    }

    int RunBluetoothPeripheralStopCommand(_In_ BluetoothPeripheralStopOptions const& options)
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const response = midi2bt::MidiBluetoothTransportManager::StopPeripheralAsync().get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_PERIPHERAL_STOP_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_PERIPHERAL_STOPPED));
        WriteBlankLine();

        if (options.Temporary)
        {
            WriteInfoLine(ResourceString(IDS_BT_TEMPORARY_NOTE));

            return 0;
        }

        // Stopping only lasts for this session unless the configuration file says so too.
        midi2bt::MidiBluetoothPeripheralConfig stopped;
        stopped.IsEnabled(false);

        ReportSave(stopped);

        return 0;
    }

    int RunBluetoothPeripheralStatusCommand()
    {
        auto const status = midi2bt::MidiBluetoothTransportManager::GetPeripheralStatus();

        if (status == nullptr)
        {
            WriteErrorLine(ResourceString(IDS_BT_NOT_AVAILABLE));
            return 1;
        }

        WriteSectionHeading(ResourceString(IDS_BT_PERIPHERAL_STATUS_TITLE));

        WriteLine(fmt::format("  {}",
            Styled(status.IsRunning()
                ? ResourceString(IDS_BT_PERIPHERAL_PUBLISHED)
                : ResourceString(IDS_BT_PERIPHERAL_NOT_PUBLISHED),
                status.IsRunning() ? successTextStyle : warningTextStyle)));

        WriteBlankLine();

        WriteField(ResourceString(IDS_LABEL_NAME), ToUtf8(status.AdvertisedName()), endpointNameTextStyle);
        WriteField(ResourceString(IDS_BT_LABEL_PROTOCOL), FormatBluetoothProtocol(status.Protocol()));
        WriteField(ResourceString(IDS_BT_LABEL_CONNECTED),
            FormatBoolean(status.IsClientConnected()), BooleanStyle(status.IsClientConnected()));

        auto const endpointDeviceId = ToUtf8(status.EndpointDeviceId());

        if (!endpointDeviceId.empty())
        {
            WriteField(ResourceString(IDS_LABEL_ID), endpointDeviceId, endpointIdTextStyle);
        }

        WriteField(ResourceString(IDS_BT_LABEL_IN_COUNTS),
            fmt::format("{} / {}", status.MessagesReceived(), status.PacketsReceived()), numberTextStyle);
        WriteField(ResourceString(IDS_BT_LABEL_OUT_COUNTS),
            fmt::format("{} / {}", status.MessagesSent(), status.PacketsSent()), numberTextStyle);

        auto const connectedClient = status.ConnectedClient();

        if (connectedClient != nullptr)
        {
            WriteBlankLine();
            WriteField(ResourceString(IDS_LABEL_ADDRESS),
                FormatBluetoothAddress(connectedClient.BluetoothAddress()));
            WriteField(ResourceString(IDS_LABEL_NAME), ToUtf8(connectedClient.Name()), endpointNameTextStyle);
        }

        WriteRememberedClients("Allowed", status.AllowedClients());
        WriteRememberedClients("Denied", status.DeniedClients());

        return 0;
    }

    int RunBluetoothPeripheralCustomizeCommand(_In_ BluetoothCustomizeOptions const& options)
    {
        auto const status = midi2bt::MidiBluetoothTransportManager::GetPeripheralStatus();

        if (status == nullptr || ToUtf8(status.EndpointDeviceId()).empty())
        {
            WriteErrorLine(ResourceString(IDS_ERROR_ENDPOINT_NOT_FOUND));
            return 1;
        }

        midi2config::MidiServiceEndpointCustomizationConfig customization;

        customization.MatchCriteria().EndpointDeviceId(status.EndpointDeviceId());

        if (options.Clear)
        {
            customization.ClearDisplayProperties(true);
        }
        else
        {
            if (options.HasName)        customization.Name(winrt::hstring{ FromUtf8(options.Name) });
            if (options.HasDescription) customization.Description(winrt::hstring{ FromUtf8(options.Description) });
            if (options.HasImage)       customization.ImageFileName(winrt::hstring{ FromUtf8(options.Image) });
        }

        if (!ApplyConfig(customization, options.Temporary))
        {
            WriteErrorLine(FormatResourceString(IDS_BT_CUSTOMIZE_FAILED, std::string{}));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_CUSTOMIZE_SUCCEEDED));

        return 0;
    }

    int RunBluetoothPeripheralApproveCommand(_In_ BluetoothPeripheralClientOptions const& options)
    {
        midi2bt::MidiBluetoothApprovalScope scope{};

        if (!TryParseApprovalScope(options.Scope, scope))
        {
            return 1;
        }

        auto const response = midi2bt::MidiBluetoothTransportManager::ApprovePeripheralClientAsync(
            winrt::hstring{ FromUtf8(options.BluetoothAddress) }, scope).get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_PERIPHERAL_COMMAND_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_PERIPHERAL_APPROVED));

        PersistClientLists(scope);

        return 0;
    }

    int RunBluetoothPeripheralDenyCommand(_In_ BluetoothPeripheralClientOptions const& options)
    {
        midi2bt::MidiBluetoothApprovalScope scope{};

        if (!TryParseApprovalScope(options.Scope, scope))
        {
            return 1;
        }

        auto const response = midi2bt::MidiBluetoothTransportManager::DenyPeripheralClientAsync(
            winrt::hstring{ FromUtf8(options.BluetoothAddress) }, scope).get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_PERIPHERAL_COMMAND_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_PERIPHERAL_DENIED));

        PersistClientLists(scope);

        return 0;
    }

    int RunBluetoothPeripheralForgetCommand(_In_ BluetoothPeripheralClientOptions const& options)
    {
        auto const response = midi2bt::MidiBluetoothTransportManager::ForgetPeripheralClientAsync(
            winrt::hstring{ FromUtf8(options.BluetoothAddress) }).get();

        if (response == nullptr || !response.Success())
        {
            auto const message = response == nullptr ? std::string{} : ToUtf8(response.ErrorMessage());

            WriteErrorLine(FormatResourceString(IDS_BT_PERIPHERAL_COMMAND_FAILED, message));
            return 1;
        }

        WriteSuccessLine(ResourceString(IDS_BT_PERIPHERAL_FORGOTTEN));

        // Forgetting is always a change to the remembered lists, so it always has to be written.
        PersistClientLists(midi2bt::MidiBluetoothApprovalScope::Always);

        return 0;
    }

    int RunBluetoothStatusCommand()
    {
        if (!EnsureTransportAvailable())
        {
            return AsExitCode(ReturnCode::ErrorGeneralFailure);
        }

        auto const radio = midi2bt::MidiBluetoothTransportManager::GetRadioInformation();

        WriteSectionHeading(ResourceString(IDS_BT_STATUS_TITLE));

        if (radio == nullptr)
        {
            WriteWarningLine(ResourceString(IDS_BT_RADIO_UNAVAILABLE));
            return 0;
        }

        WriteField(ResourceString(IDS_BT_LABEL_RADIO_PRESENT),
            FormatBoolean(radio.IsPresent()), BooleanStyle(radio.IsPresent()));
        WriteField(ResourceString(IDS_BT_LABEL_LOW_ENERGY_SUPPORTED),
            FormatBoolean(radio.IsLowEnergySupported()), BooleanStyle(radio.IsLowEnergySupported()));
        WriteField(ResourceString(IDS_BT_LABEL_RADIO_ENABLED),
            FormatBoolean(radio.IsCentralRoleSupported()), BooleanStyle(radio.IsCentralRoleSupported()));
        WriteField(ResourceString(IDS_BT_LABEL_PERIPHERAL_SUPPORTED),
            FormatBoolean(radio.IsPeripheralRoleSupported()), BooleanStyle(radio.IsPeripheralRoleSupported()));

        auto const defaultRetention = midi2bt::MidiBluetoothTransportManager::GetDefaultOfflineRetentionSeconds();

        WriteField(ResourceString(IDS_BT_LABEL_DEFAULT_RETENTION),
            FormatOfflineRetention(defaultRetention), fieldValueTextStyle);

        WriteBlankLine();

        ReportRadioLimitations();

        auto const pending = midi2bt::MidiBluetoothTransportManager::GetPendingPeripheralClients();

        if (pending == nullptr || pending.Size() == 0)
        {
            WriteInfoLine(ResourceString(IDS_BT_NO_PENDING));

            return 0;
        }

        ConsoleTable table{ ResourceString(IDS_BT_PENDING_TABLE_TITLE) };

        table.AddColumn(ResourceString(IDS_LABEL_ADDRESS), ColumnAlignment::Left, endpointIdTextStyle);
        table.AddColumn(ResourceString(IDS_LABEL_NAME), ColumnAlignment::Left, endpointNameTextStyle);
        table.SetLastColumnShrinkable();
        table.AddColumn(ResourceString(IDS_BT_LABEL_PAIRED));

        for (auto const& client : pending)
        {
            table.BeginRow();
            table.AddCell(FormatBluetoothAddress(client.BluetoothAddress()));
            table.AddCell(ToUtf8(client.Name()));
            table.AddCell(FormatBoolean(client.IsPaired()), BooleanStyle(client.IsPaired()));
        }

        table.Render();

        return 0;
    }
}
