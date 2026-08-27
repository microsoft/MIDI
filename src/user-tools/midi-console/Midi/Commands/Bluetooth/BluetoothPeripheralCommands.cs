// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.Devices.Midi2.Transports.Bluetooth;

namespace Microsoft.Midi.ConsoleApp
{
    internal static class BluetoothPeripheral
    {
        internal const string ProtocolMidi1 = "midi1";
        internal const string ProtocolMidi2 = "midi2";

        internal static MidiBluetoothProtocol ResolveProtocol(string? value)
        {
            return string.Equals(value, ProtocolMidi2, StringComparison.OrdinalIgnoreCase)
                ? MidiBluetoothProtocol.BluetoothLowEnergyMidi2Ump
                : MidiBluetoothProtocol.BluetoothLowEnergyMidi1;
        }

        // Not every radio can advertise, so whether this PC could ever be published is worth
        // stating before anything about whether it currently is.
        private static string DescribeRadio()
        {
            var radio = MidiBluetoothTransportManager.GetRadioInformation();

            if (radio == null)
            {
                return "[grey](not reported)[/]";
            }

            if (!radio.IsPresent)
            {
                return "[red]none[/]";
            }

            if (!radio.IsLowEnergySupported)
            {
                return "[red]no Bluetooth Low Energy[/]";
            }

            var roles = new List<string>();

            if (radio.IsCentralRoleSupported) roles.Add("central");
            if (radio.IsPeripheralRoleSupported) roles.Add("peripheral");

            return roles.Count > 0 ? string.Join(" + ", roles) : "[red]no usable role[/]";
        }

        private static void ReportRadio()
        {
            var radio = MidiBluetoothTransportManager.GetRadioInformation();

            if (radio == null)
            {
                return;
            }

            if (!radio.IsPresent)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This PC has no Bluetooth radio."));
                AnsiConsole.WriteLine();
                return;
            }

            if (!radio.IsLowEnergySupported)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This PC's Bluetooth radio does not support Bluetooth Low Energy, which Bluetooth MIDI requires."));
                AnsiConsole.WriteLine();
                return;
            }

            if (!radio.IsPeripheralRoleSupported)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This PC's Bluetooth radio cannot act as a peripheral, so this PC cannot be published for other devices to connect to."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Connecting out to Bluetooth MIDI devices still works."));
                AnsiConsole.WriteLine();
            }
        }

        internal static void ReportStatus(MidiBluetoothPeripheralStatus status)
        {
            if (status == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("The transport did not report the peripheral status."));
                return;
            }

            ReportRadio();

            if (!status.IsRunning)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This PC is not published as a Bluetooth MIDI device."));
                return;
            }

            var client = status.ConnectedClient;

            var table = new Table();

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Property"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Value"));

            table.AddRow("Published as", string.IsNullOrEmpty(status.AdvertisedName) ? "[grey](unknown)[/]" : AnsiMarkupFormatter.EscapeString(status.AdvertisedName));
            table.AddRow("Protocol", BluetoothTransport.FormatProtocol(status.Protocol));
            table.AddRow("Radio", DescribeRadio());
            table.AddRow("Connected device", status.IsClientConnected && client != null && !string.IsNullOrEmpty(client.Name)
                ? AnsiMarkupFormatter.FormatEndpointName(client.Name)
                : "[grey]none[/]");

            if (status.IsClientConnected && client != null)
            {
                table.AddRow("Paired", client.IsPaired ? "[green]yes[/]" : "no");

                // A random address is rotated by the device for privacy, so it is not an identity.
                table.AddRow("Address", client.BluetoothAddress == 0
                    ? "-"
                    : $"{client.BluetoothAddress:X12} ({client.BluetoothAddressType})");

                // The remote device is the Central here, so it chose this.
                table.AddRow("Connection interval", client.ConnectionInterval > TimeSpan.Zero
                    ? $"{client.ConnectionInterval.TotalMilliseconds:N2} ms"
                    : "-");
            }

            table.AddRow("Subscribed clients", status.SubscribedClientCount.ToString());
            table.AddRow("Messages in", status.IsClientConnected ? status.MessagesReceived.ToString() : "-");
            table.AddRow("Messages out", status.IsClientConnected ? status.MessagesSent.ToString() : "-");

            if (!string.IsNullOrEmpty(status.EndpointDeviceId))
            {
                table.AddRow("Endpoint", AnsiMarkupFormatter.FormatFullEndpointInterfaceId(status.EndpointDeviceId));
            }

            AnsiConsole.Write(table);
            AnsiConsole.WriteLine();

            // The endpoint is the remote device, the same way a Network MIDI 2.0 host endpoint is
            // the remote client, so there is nothing to open until something connects.
            if (!status.IsClientConnected)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "Nothing is connected yet. A MIDI endpoint appears when a device connects to this PC."));
                AnsiConsole.WriteLine();
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "Use 'midi bluetooth peripheral customize' to rename this endpoint."));
                AnsiConsole.WriteLine();
            }

            if (status.IsClientConnected && client != null && !client.IsPaired)
            {
                if (client.HasGenericName)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                        $"'{AnsiMarkupFormatter.EscapeString(client.Name)}' is a generic name. Phones and tablets withhold their real name from an unpaired PC."));
                }

                // Pairing has to be initiated from the remote device, which is the side that
                // scanned for and connected to this PC.
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                    "On the device itself, pair with this PC. That releases its real name and gives it a stable identity."));
                AnsiConsole.WriteLine();
            }

            // Windows takes the advertised Bluetooth name from the computer name, and the GATT
            // service provider gives an application no way to override it.
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                "The name a remote device sees is this PC's Bluetooth name, which is changed in Windows Settings, not here."));
        }
    }


    internal class BluetoothPeripheralStartCommand : Command<BluetoothPeripheralStartCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothPeripheralProtocol")]
            [CommandOption("-p|--protocol")]
            public string? Protocol { get; set; }

            [LocalizedDescription("ParameterBluetoothTemporary")]
            [CommandOption("-t|--temporary")]
            public bool Temporary { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (settings.Protocol != null &&
                !string.Equals(settings.Protocol, BluetoothPeripheral.ProtocolMidi1, StringComparison.OrdinalIgnoreCase) &&
                !string.Equals(settings.Protocol, BluetoothPeripheral.ProtocolMidi2, StringComparison.OrdinalIgnoreCase))
            {
                return ValidationResult.Error($"The protocol must be '{BluetoothPeripheral.ProtocolMidi1}' or '{BluetoothPeripheral.ProtocolMidi2}'.");
            }

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var protocol = BluetoothPeripheral.ResolveProtocol(settings.Protocol);

            var config = new MidiBluetoothPeripheralConfig(protocol);

            var response = AnsiConsole.Status()
                .Start("Starting...", ctx => MidiBluetoothTransportManager.StartPeripheralAsync(config).GetAwaiter().GetResult());

            if (!response.Success)
            {
                BluetoothTransport.ReportFailure(response.ErrorCode.ToString(), response.ErrorMessage, response.ErrorHResult);

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("This PC is now published as a Bluetooth MIDI device."));
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                "A MIDI endpoint is created when a device connects, and is named after that device."));
            AnsiConsole.WriteLine();

            BluetoothPeripheral.ReportStatus(response.Status);

            if (protocol == MidiBluetoothProtocol.BluetoothLowEnergyMidi2Ump)
            {
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                    "BLE MIDI 2.0 is a draft standard and almost nothing supports it yet. Only one protocol can be published at a time, so a MIDI 1.0 device will not be able to connect while this is selected."));
            }

            if (settings.Temporary)
            {
                return (int)MidiConsoleReturnCode.Success;
            }

            AnsiConsole.WriteLine();

            ConfigFileSaver.ReportSave(config);

            return (int)MidiConsoleReturnCode.Success;
        }
    }


    internal class BluetoothPeripheralStopCommand : Command<BluetoothPeripheralStopCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothTemporary")]
            [CommandOption("-t|--temporary")]
            public bool Temporary { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var response = AnsiConsole.Status()
                .Start("Stopping...", ctx => MidiBluetoothTransportManager.StopPeripheralAsync().GetAwaiter().GetResult());

            if (!response.Success)
            {
                BluetoothTransport.ReportFailure(response.ErrorCode.ToString(), response.ErrorMessage, response.ErrorHResult);

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("This PC is no longer published as a Bluetooth MIDI device."));
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Any MIDI endpoint for a connected device is removed. An app still using it will see the endpoint go away."));

            if (settings.Temporary)
            {
                return (int)MidiConsoleReturnCode.Success;
            }

            AnsiConsole.WriteLine();

            // Stopping only lasts for this session unless the configuration file says so too
            ConfigFileSaver.ReportSave(new MidiBluetoothPeripheralConfig { IsEnabled = false });

            return (int)MidiConsoleReturnCode.Success;
        }
    }


    internal class BluetoothPeripheralStatusCommand : Command<BluetoothPeripheralStatusCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            BluetoothPeripheral.ReportStatus(MidiBluetoothTransportManager.GetPeripheralStatus());

            return (int)MidiConsoleReturnCode.Success;
        }
    }


    internal class BluetoothPeripheralCustomizeCommand : Command<BluetoothPeripheralCustomizeCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothCustomName")]
            [CommandOption("-n|--name")]
            public string? Name { get; set; }

            [LocalizedDescription("ParameterBluetoothCustomDescription")]
            [CommandOption("-d|--description")]
            public string? Description { get; set; }

            [LocalizedDescription("ParameterBluetoothCustomImage")]
            [CommandOption("-i|--image")]
            public string? Image { get; set; }

            [LocalizedDescription("ParameterBluetoothCustomClear")]
            [CommandOption("-c|--clear")]
            public bool Clear { get; set; }

            [LocalizedDescription("ParameterBluetoothTemporary")]
            [CommandOption("-t|--temporary")]
            [DefaultValue(false)]
            public bool Temporary { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            return BluetoothTransport.ValidateCustomizationOptions(
                settings.Name, settings.Description, settings.Image, settings.Clear);
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var status = MidiBluetoothTransportManager.GetPeripheralStatus();

            // The endpoint is the remote device, and its identity is what the customization is
            // keyed on, so there is nothing to address until something is connected.
            if (status == null || string.IsNullOrEmpty(status.EndpointDeviceInstanceId))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("There is no connected device to customize."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "Connect the device to this PC first. The customization is remembered against that device and is reapplied every time it reconnects."));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            if (!BluetoothTransport.ApplyCustomization(
                    status.EndpointDeviceInstanceId,
                    settings.Name,
                    settings.Description,
                    settings.Image,
                    settings.Clear,
                    !settings.Temporary))
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(
                settings.Clear ? "Customization removed." : "Customization applied."));

            AnsiConsole.WriteLine();

            if (status.ConnectedClient != null && !status.ConnectedClient.IsPaired)
            {
                // Without a bond there is no stable identity, so every unpaired device lands on
                // the same endpoint and shares whatever name is set here.
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                    "This device is not paired with this PC, so it has no stable identity. This customization will apply to any unpaired device which connects."));
                AnsiConsole.WriteLine();
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
