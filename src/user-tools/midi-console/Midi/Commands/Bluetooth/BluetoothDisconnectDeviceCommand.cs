// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    internal class BluetoothDisconnectDeviceCommand : Command<BluetoothDisconnectDeviceCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothDeviceId")]
            [CommandArgument(0, "<device id>")]
            public string? DeviceId { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.DeviceId))
            {
                return ValidationResult.Error("Missing device id. Use 'midi bluetooth list' to see the connected devices.");
            }

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            var responseJson = BluetoothTransport.SendCommand(BluetoothTransport.VerbDisconnectDevice, settings.DeviceId!.Trim());

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Disconnection requested."));
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("The MIDI endpoint for this device is removed. Any app still connected to it will see the endpoint go away."));

            // Leaving the entry behind would silently reconnect the device on the next service
            // start, which is not what disconnecting means.
            if (BluetoothConfigFile.TryRemoveDevice(settings.DeviceId!.Trim(), out var error))
            {
                return (int)MidiConsoleReturnCode.Success;
            }

            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(AnsiMarkupFormatter.EscapeString(error ?? "The saved entry could not be removed.")));
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This device may reconnect when the service restarts."));

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
