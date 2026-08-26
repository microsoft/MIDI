// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace Microsoft.Midi.ConsoleApp
{
    internal class BluetoothConnectionParametersCommand : Command<BluetoothConnectionParametersCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothConnectionParameters")]
            [CommandArgument(0, "<preference>")]
            public string? Preference { get; set; }

            [LocalizedDescription("ParameterBluetoothTemporary")]
            [CommandOption("-t|--temporary")]
            public bool Temporary { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (BluetoothTransport.ResolveConnectionParameterPreference(settings.Preference) == null)
            {
                return ValidationResult.Error(
                    "The preference must be 'system', 'throughput', 'balanced', or 'power'.");
            }

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            var preference = BluetoothTransport.ResolveConnectionParameterPreference(settings.Preference)!;

            var responseJson = BluetoothTransport.SendConnectionParametersCommand(preference);

            if (responseJson == null)
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var applied = responseJson.GetNamedString(BluetoothTransport.ArgumentConnectionParameters, preference);
            var min = responseJson.GetNamedNumber(BluetoothTransport.ResponseMinIntervalMilliseconds, 0);
            var max = responseJson.GetNamedNumber(BluetoothTransport.ResponseMaxIntervalMilliseconds, 0);

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess($"Connection parameter preference set to '{applied}'."));
            AnsiConsole.WriteLine();

            // The preset names say nothing about the interval they ask for, so it is shown.
            if (min > 0 || max > 0)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    $"This asks the radio for a connection interval between {min:N2} ms and {max:N2} ms."));
            }
            else
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "No request will be made, leaving the interval entirely to Windows and the device."));
            }

            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                "This applies when a device next connects, so disconnect and reconnect a device to compare."));

            if (settings.Temporary)
            {
                return (int)MidiConsoleReturnCode.Success;
            }

            if (!BluetoothConfigFile.TrySetConnectionParameters(preference, out var error))
            {
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(AnsiMarkupFormatter.EscapeString(error ?? "The setting could not be saved.")));
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
