// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    internal class BluetoothConnectDeviceCommand : Command<BluetoothConnectDeviceCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothDeviceId")]
            [CommandArgument(0, "<device id>")]
            public string? DeviceId { get; set; }

            [LocalizedDescription("ParameterBluetoothTemporary")]
            [CommandOption("-t|--temporary")]
            [DefaultValue(false)]
            public bool Temporary { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.DeviceId))
            {
                return ValidationResult.Error("Missing device id. Use 'midi bluetooth list' to see the available devices.");
            }

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            var responseJson = BluetoothTransport.SendCommand(BluetoothTransport.VerbConnectDevice, settings.DeviceId!.Trim());

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var isKnown = responseJson.GetNamedBoolean("isKnown", false);
            var isPresent = responseJson.GetNamedBoolean("isPresent", false);
            var name = responseJson.GetNamedString("name", string.Empty);

            var displayName = string.IsNullOrEmpty(name) ? settings.DeviceId!.Trim() : name;

            // The request is remembered rather than performed, so saying "connecting" for a
            // device that is powered off would be a lie.
            if (isPresent)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess($"Connecting to {AnsiMarkupFormatter.EscapeString(displayName)}."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("The connection is made in the background. Use 'midi bluetooth list' to see when it reports as connected, and 'midi enumerate midi-services-endpoints' to see the new endpoint."));
            }
            else if (isKnown)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess($"{AnsiMarkupFormatter.EscapeString(displayName)} has been remembered."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This device is not in range right now, so nothing is connected yet."));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("It will connect on its own, and its endpoint will be created, as soon as it is switched on and in range."));
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess($"{AnsiMarkupFormatter.EscapeString(displayName)} has been remembered."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This device has not been discovered, so nothing is connected yet."));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("It will connect on its own if a device with this id appears. Check the id with 'midi bluetooth list' if that does not happen."));
            }

            AnsiConsole.WriteLine();

            if (settings.Temporary)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This device was not saved to the configuration file, so it is forgotten when the service restarts."));
            }
            else if (BluetoothConfigFile.TryAddDevice(settings.DeviceId!.Trim(), out var error))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    $"Saved to {AnsiMarkupFormatter.EscapeString(BluetoothConfigFile.FilePath ?? string.Empty)}, so it reconnects after a service restart."));
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(AnsiMarkupFormatter.EscapeString(error ?? "The device could not be saved.")));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("The connection still works, but is forgotten when the service restarts."));
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
