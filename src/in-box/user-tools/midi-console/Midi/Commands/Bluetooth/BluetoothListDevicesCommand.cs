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
    internal class BluetoothListDevicesCommand : Command<BluetoothListDevicesCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        // A connected device stops advertising, so its presence comes from the link. For the
        // rest, how long ago it was last heard from is the only presence signal BLE offers.
        private static string FormatPresence(bool isConnected, bool isPresent, bool hasBeenSeen, TimeSpan lastSeenAgo)
        {
            if (isConnected)
            {
                return "[green]connected[/]";
            }

            if (isPresent)
            {
                return "nearby";
            }

            // A paired device the system remembers has never been heard by the radio, so there is
            // no age to report.
            if (!hasBeenSeen)
            {
                return "[grey]not heard yet[/]";
            }

            var seconds = (long)lastSeenAgo.TotalSeconds;

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

        // -127 is the sentinel for no reading rather than an extremely weak one, and a connected
        // device has stopped advertising so there is nothing left to measure.
        private static string FormatSignalStrength(short decibelMilliwatts)
        {
            if (decibelMilliwatts == 0 || decibelMilliwatts <= -127)
            {
                return "-";
            }

            return $"{decibelMilliwatts} dBm";
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var devices = MidiBluetoothTransportManager.GetAvailableDevices();

            if (devices.Count == 0)
            {
                BluetoothTransport.ReportRadioLimitations();

                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("No Bluetooth MIDI devices have been discovered."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Devices are found by listening for advertisements, so a device which is powered off, asleep, or already connected to another host will not be listed. Wake the device and try again."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("A device is also held back for a few seconds after it is first seen, until its name is known."));

                return (int)MidiConsoleReturnCode.Success;
            }

            var table = new Table();

            AnsiMarkupFormatter.SetTableBorderStyle(table);

            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Bluetooth Device Id"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Name"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Protocol"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Endpoint"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Presence"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Interval"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Signal"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("In (msg/pkt)"));
            table.AddColumn(AnsiMarkupFormatter.FormatTableColumnHeading("Out (msg/pkt)"));

            var connectErrors = new List<string>();

            foreach (var device in devices)
            {
                table.AddRow(
                    AnsiMarkupFormatter.FormatDeviceInstanceId(device.BluetoothDeviceId),
                    string.IsNullOrEmpty(device.Name) ? "[grey](unknown name)[/]" : AnsiMarkupFormatter.FormatEndpointName(device.Name),
                    BluetoothTransport.FormatProtocol(device.SelectedProtocol),
                    device.HasEndpoint ? "[green]yes[/]" : "no",
                    FormatPresence(device.IsConnected, device.IsPresent, device.HasBeenSeen, device.LastSeenAgo),
                    device.ConnectionInterval > TimeSpan.Zero ? $"{device.ConnectionInterval.TotalMilliseconds:N2} ms" : "-",
                    FormatSignalStrength(device.SignalStrengthDecibelMilliwatts),
                    device.HasEndpoint ? $"{device.MessagesReceived} / {device.PacketsReceived}" : "-",
                    device.HasEndpoint ? $"{device.MessagesSent} / {device.PacketsSent}" : "-");

                if (!string.IsNullOrEmpty(device.LastConnectError))
                {
                    connectErrors.Add($"{device.BluetoothDeviceId}: {device.LastConnectError}");
                }

                if (device.LastSendErrorHResult != 0)
                {
                    connectErrors.Add($"{device.BluetoothDeviceId}: the most recent send to this device failed (0x{device.LastSendErrorHResult:X8}).");
                }
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

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Use 'midi bluetooth connect <bluetooth device id>' to create a MIDI endpoint for a device."));

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
