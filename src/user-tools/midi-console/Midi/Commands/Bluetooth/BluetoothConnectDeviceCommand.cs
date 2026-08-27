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
    internal class BluetoothConnectDeviceCommand : Command<BluetoothConnectDeviceCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothDeviceId")]
            [CommandArgument(0, "<bluetooth device id>")]
            public string? BluetoothDeviceId { get; set; }

            [LocalizedDescription("ParameterBluetoothTemporary")]
            [CommandOption("-t|--temporary")]
            [DefaultValue(false)]
            public bool Temporary { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.BluetoothDeviceId))
            {
                return ValidationResult.Error("Missing Bluetooth device id. Use 'midi bluetooth list' to see the available devices.");
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

            var config = new MidiBluetoothDeviceConnectConfig(bluetoothDeviceId);

            var response = AnsiConsole.Status()
                .Start("Connecting...", ctx => MidiBluetoothTransportManager.ConnectDeviceAsync(config).GetAwaiter().GetResult());

            if (!response.Success)
            {
                BluetoothTransport.ReportFailure(response.ErrorCode.ToString(), response.ErrorMessage, response.ErrorHResult);

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var displayName = response.Device != null && !string.IsNullOrEmpty(response.Device.Name)
                ? response.Device.Name
                : bluetoothDeviceId;

            // The request is remembered rather than performed, so saying "connecting" for a
            // device that is powered off would be a lie.
            if (response.Device != null && response.Device.IsPresent)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess($"Connecting to {AnsiMarkupFormatter.EscapeString(displayName)}."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("The connection is made in the background. Use 'midi bluetooth list' to see when it reports as connected, and 'midi enumerate midi-services-endpoints' to see the new endpoint."));
            }
            else if (response.IsKnown)
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

                return (int)MidiConsoleReturnCode.Success;
            }

            ConfigFileSaver.ReportSave(config);

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
