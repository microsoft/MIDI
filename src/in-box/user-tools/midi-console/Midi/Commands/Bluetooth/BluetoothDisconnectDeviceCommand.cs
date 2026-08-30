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
    internal class BluetoothDisconnectDeviceCommand : Command<BluetoothDisconnectDeviceCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothDeviceId")]
            [CommandArgument(0, "<bluetooth device id>")]
            public string? BluetoothDeviceId { get; set; }

            [LocalizedDescription("ParameterBluetoothForget")]
            [CommandOption("-f|--forget")]
            [DefaultValue(false)]
            public bool Forget { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.BluetoothDeviceId))
            {
                return ValidationResult.Error("Missing Bluetooth device id. Use 'midi bluetooth list' to see the connected devices.");
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

            var bluetoothDeviceId = settings.BluetoothDeviceId!.Trim();

            var config = new MidiBluetoothDeviceDisconnectConfig(bluetoothDeviceId, settings.Forget);

            var response = AnsiConsole.Status()
                .Start("Disconnecting...", ctx => MidiBluetoothTransportManager.DisconnectDeviceAsync(config).GetAwaiter().GetResult());

            // Forgetting a device is still meaningful when it is not connected, which is exactly
            // when someone is most likely to want it, so only a real failure stops here.
            var wasConnected = response.Success;

            if (!response.Success &&
                response.ErrorCode != MidiBluetoothDeviceDisconnectErrorCode.NotConnected &&
                response.ErrorCode != MidiBluetoothDeviceDisconnectErrorCode.DeviceNotDiscovered)
            {
                BluetoothTransport.ReportFailure(response.ErrorCode.ToString(), response.ErrorMessage, response.ErrorHResult);

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            if (wasConnected)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Disconnection requested."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("The MIDI endpoint for this device is removed. Any app still connected to it will see the endpoint go away."));
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This device was not connected."));
            }

            AnsiConsole.WriteLine();

            // Saving records that this device should no longer connect on its own. Without it the
            // device is reconnected the next time the service starts.
            ConfigFileSaver.ReportSave(config);

            if (settings.Forget)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This device has been removed from the configuration file."));
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This device is still listed in the configuration file, but will not connect on its own. Use --forget to remove it entirely."));
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
