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
    internal class BluetoothCustomizeDeviceCommand : Command<BluetoothCustomizeDeviceCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothDeviceId")]
            [CommandArgument(0, "<bluetooth device id>")]
            public string? BluetoothDeviceId { get; set; }

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
            if (string.IsNullOrWhiteSpace(settings.BluetoothDeviceId))
            {
                return ValidationResult.Error("Missing Bluetooth device id. Use 'midi bluetooth list' to see the discovered devices.");
            }

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

            var bluetoothDeviceId = settings.BluetoothDeviceId!.Trim();

            var device = MidiBluetoothTransportManager.GetDevice(bluetoothDeviceId);

            if (device == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Device {AnsiMarkupFormatter.EscapeString(bluetoothDeviceId)} has not been discovered."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Wake the device so it advertises, then use 'midi bluetooth list' to confirm the Bluetooth device id."));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            // The endpoint is matched on its instance id, which the transport derives from the
            // device, so a customization can be written before the device has ever connected.
            if (string.IsNullOrEmpty(device.EndpointDeviceInstanceId))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("This device has no endpoint instance id, so it cannot be customized."));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            if (!BluetoothTransport.ApplyCustomization(
                    device.EndpointDeviceInstanceId,
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

            if (!string.IsNullOrEmpty(device.Name))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    $"The name reported by the device is {AnsiMarkupFormatter.EscapeString(device.Name)}."));
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
