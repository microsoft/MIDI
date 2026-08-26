// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    internal class BluetoothListDevicesCommand : Command<BluetoothListDevicesCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        // A connected device stops advertising, so its presence comes from the link. For the
        // rest, how long ago it was last heard from is the only presence signal BLE offers.
        private static string FormatPresence(bool isConnected, bool isPresent, ulong lastSeenAgoMilliseconds)
        {
            if (isConnected)
            {
                return "[green]connected[/]";
            }

            if (isPresent)
            {
                return "nearby";
            }

            var seconds = lastSeenAgoMilliseconds / 1000;

            if (seconds == 0)
            {
                return "[grey]away[/]";
            }

            if (seconds < 120)
            {
                return $"[grey]away ({seconds}s)[/]";
            }

            var minutes = seconds / 60;

            if (minutes < 120)
            {
                return $"[grey]away ({minutes}m)[/]";
            }

            return $"[grey]away ({minutes / 60}h)[/]";
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            var responseJson = BluetoothTransport.SendCommand(BluetoothTransport.VerbListAvailableDevices);

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var table = new Table();

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Device Id"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Name"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Protocol"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Endpoint"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Presence"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Interval"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Signal"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Msgs In"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Msgs Out"));

            uint deviceCount = 0;

            var connectErrors = new List<string>();

            if (responseJson.ContainsKey("availableDevices"))
            {
                foreach (var entry in responseJson.GetNamedArray("availableDevices"))
                {
                    var device = entry.GetObject();

                    var deviceId = device.GetNamedString("deviceId", string.Empty);
                    var name = device.GetNamedString("name", string.Empty);
                    var protocol = device.GetNamedString("selectedProtocol", string.Empty);
                    var isConnected = device.GetNamedBoolean("isConnected", false);
                    var signal = (int)device.GetNamedNumber("signalStrengthDbm", 0);
                    var lastConnectError = device.GetNamedString("lastConnectError", string.Empty);
                    var messagesIn = (ulong)device.GetNamedNumber("messagesReceived", 0);
                    var messagesOut = (ulong)device.GetNamedNumber("messagesSent", 0);
                    var lastSendError = (int)device.GetNamedNumber("lastSendErrorHresult", 0);
                    var isPresent = device.GetNamedBoolean("isPresent", false);
                    var lastSeenAgo = (ulong)device.GetNamedNumber("lastSeenAgoMilliseconds", 0);
                    var hasEndpoint = device.GetNamedBoolean("hasEndpoint", false);
                    var intervalMilliseconds = device.GetNamedNumber("connectionIntervalMilliseconds", 0);

                    // A device only reports its protocol once connected, because that requires
                    // reading its characteristics.
                    if (string.IsNullOrEmpty(protocol) || protocol == "unknown")
                    {
                        protocol = "-";
                    }

                    table.AddRow(
                        AnsiMarkupFormatter.FormatDeviceInstanceId(deviceId),
                        string.IsNullOrEmpty(name) ? "[grey](unknown name)[/]" : AnsiMarkupFormatter.FormatEndpointName(name),
                        protocol,
                        hasEndpoint ? "[green]yes[/]" : "no",
                        FormatPresence(isConnected, isPresent, lastSeenAgo),
                        intervalMilliseconds > 0 ? $"{intervalMilliseconds:N2} ms" : "-",
                        signal != 0 ? $"{signal} dBm" : "-",
                        hasEndpoint ? messagesIn.ToString() : "-",
                        hasEndpoint ? messagesOut.ToString() : "-");

                    if (!string.IsNullOrEmpty(lastConnectError))
                    {
                        connectErrors.Add($"{deviceId}: {lastConnectError}");
                    }

                    if (lastSendError != 0)
                    {
                        connectErrors.Add($"{deviceId}: the most recent send to this device failed (0x{lastSendError:X8}).");
                    }

                    deviceCount++;
                }
            }

            if (deviceCount == 0)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("No Bluetooth MIDI devices have been discovered."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Devices are found by listening for advertisements, so a device which is powered off, asleep, or already connected to another host will not be listed. Wake the device and try again."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("A device is also held back for a few seconds after it is first seen, until its name is known."));

                return (int)MidiConsoleReturnCode.Success;
            }

            AnsiConsole.Write(table);
            AnsiConsole.WriteLine();

            if (connectErrors.Count > 0)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("Most recent connection failures:"));

                foreach (var error in connectErrors)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("  " + AnsiMarkupFormatter.EscapeString(error)));
                }

                AnsiConsole.WriteLine();
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Use 'midi bluetooth connect <device id>' to create a MIDI endpoint for a device."));

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
