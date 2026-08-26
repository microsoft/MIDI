// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.Data.Json;

namespace Microsoft.Midi.ConsoleApp
{
    internal static class BluetoothPeripheral
    {
        internal const string ProtocolMidi1 = "midi1";
        internal const string ProtocolMidi2 = "midi2";

        internal static void ReportStatus(JsonObject responseJson)
        {
            if (!responseJson.ContainsKey(BluetoothTransport.ResponsePeripheral))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("The transport did not report the peripheral status."));
                return;
            }

            var peripheral = responseJson.GetNamedObject(BluetoothTransport.ResponsePeripheral);

            var isRunning = peripheral.GetNamedBoolean(BluetoothTransport.ResponsePeripheralIsRunning, false);

            if (!isRunning)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This PC is not published as a Bluetooth MIDI device."));
                return;
            }

            var advertisedName = peripheral.GetNamedString(BluetoothTransport.ResponsePeripheralAdvertisedName, string.Empty);
            var protocol = peripheral.GetNamedString(BluetoothTransport.ResponsePeripheralProtocol, string.Empty);
            var connectedDeviceName = peripheral.GetNamedString(BluetoothTransport.ResponseName, string.Empty);
            var endpointDeviceId = peripheral.GetNamedString(BluetoothTransport.ResponseEndpointDeviceId, string.Empty);
            var isConnected = peripheral.GetNamedBoolean(BluetoothTransport.ResponseIsConnected, false);
            var clientCount = (int)peripheral.GetNamedNumber(BluetoothTransport.ResponsePeripheralClientCount, 0);
            var messagesIn = (ulong)peripheral.GetNamedNumber(BluetoothTransport.ResponseMessagesReceived, 0);
            var messagesOut = (ulong)peripheral.GetNamedNumber(BluetoothTransport.ResponseMessagesSent, 0);
            var isPaired = peripheral.GetNamedBoolean(BluetoothTransport.ResponseIsPaired, false);
            var hasGenericName = peripheral.GetNamedBoolean(BluetoothTransport.ResponseHasGenericName, false);
            var address = peripheral.GetNamedString(BluetoothTransport.ResponseBluetoothAddress, string.Empty);
            var addressType = peripheral.GetNamedString(BluetoothTransport.ResponseBluetoothAddressType, string.Empty);

            var table = new Table();

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Property"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Value"));

            table.AddRow("Published as", string.IsNullOrEmpty(advertisedName) ? "[grey](unknown)[/]" : AnsiMarkupFormatter.EscapeString(advertisedName));
            table.AddRow("Protocol", string.IsNullOrEmpty(protocol) ? "-" : protocol);
            table.AddRow("Connected device", isConnected && !string.IsNullOrEmpty(connectedDeviceName)
                ? AnsiMarkupFormatter.FormatEndpointName(connectedDeviceName)
                : "[grey]none[/]");

            if (isConnected)
            {
                table.AddRow("Paired", isPaired ? "[green]yes[/]" : "no");

                // A random address is rotated by the device for privacy, so it is not an identity.
                table.AddRow("Address", string.IsNullOrEmpty(address)
                    ? "-"
                    : $"{AnsiMarkupFormatter.EscapeString(address)}{(string.IsNullOrEmpty(addressType) ? "" : $" ({addressType})")}");

                // The remote device is the Central here, so it chose this.
                var interval = peripheral.GetNamedNumber(BluetoothTransport.ResponseConnectionIntervalMilliseconds, 0);

                table.AddRow("Connection interval", interval > 0 ? $"{interval:N2} ms" : "-");
            }

            table.AddRow("Subscribed clients", clientCount.ToString());
            table.AddRow("Messages in", isConnected ? messagesIn.ToString() : "-");
            table.AddRow("Messages out", isConnected ? messagesOut.ToString() : "-");

            if (!string.IsNullOrEmpty(endpointDeviceId))
            {
                table.AddRow("Endpoint", AnsiMarkupFormatter.FormatFullEndpointInterfaceId(endpointDeviceId));
            }

            AnsiConsole.Write(table);
            AnsiConsole.WriteLine();

            // The endpoint is the remote device, the same way a Network MIDI 2.0 host endpoint is
            // the remote client, so there is nothing to open until something connects.
            if (!isConnected)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "Nothing is connected yet. A MIDI endpoint appears when a device connects to this PC."));
                AnsiConsole.WriteLine();
            }

            if (isConnected && !isPaired)
            {
                if (hasGenericName)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                        $"'{AnsiMarkupFormatter.EscapeString(connectedDeviceName)}' is a generic name. Phones and tablets withhold their real name from an unpaired PC."));
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

            var useMidi2 = string.Equals(settings.Protocol, BluetoothPeripheral.ProtocolMidi2, StringComparison.OrdinalIgnoreCase);

            var responseJson = BluetoothTransport.SendPeripheralCommand(BluetoothTransport.VerbStartPeripheral, useMidi2);

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("This PC is now published as a Bluetooth MIDI device."));
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                "A MIDI endpoint is created when a device connects, and is named after that device."));
            AnsiConsole.WriteLine();

            BluetoothPeripheral.ReportStatus(responseJson);

            if (useMidi2)
            {
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                    "BLE MIDI 2.0 is a draft standard and almost nothing supports it yet. Only one protocol can be published at a time, so a MIDI 1.0 device will not be able to connect while this is selected."));
            }

            if (settings.Temporary)
            {
                return (int)MidiConsoleReturnCode.Success;
            }

            if (!BluetoothConfigFile.TrySetPeripheral(true, useMidi2, out var error))
            {
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(AnsiMarkupFormatter.EscapeString(error ?? "The setting could not be saved.")));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This PC will stop being published when the service restarts."));
            }

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

            var responseJson = BluetoothTransport.SendCommand(BluetoothTransport.VerbStopPeripheral);

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("This PC is no longer published as a Bluetooth MIDI device."));
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Any MIDI endpoint for a connected device is removed. An app still using it will see the endpoint go away."));

            if (settings.Temporary)
            {
                return (int)MidiConsoleReturnCode.Success;
            }

            if (!BluetoothConfigFile.TrySetPeripheral(false, false, out var error))
            {
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(AnsiMarkupFormatter.EscapeString(error ?? "The setting could not be saved.")));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This PC may be published again when the service restarts."));
            }

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

            var responseJson = BluetoothTransport.SendCommand(BluetoothTransport.VerbGetPeripheralStatus);

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            BluetoothPeripheral.ReportStatus(responseJson);

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
