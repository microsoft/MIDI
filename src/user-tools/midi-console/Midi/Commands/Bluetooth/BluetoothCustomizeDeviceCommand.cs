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
    internal class BluetoothCustomizeDeviceCommand : Command<BluetoothCustomizeDeviceCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothDeviceId")]
            [CommandArgument(0, "<device id>")]
            public string? DeviceId { get; set; }

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
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.DeviceId))
            {
                return ValidationResult.Error("Missing device id. Use 'midi bluetooth list' to see the discovered devices.");
            }

            return BluetoothTransport.ValidateCustomizationOptions(
                settings.Name, settings.Description, settings.Image, settings.Clear);
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            LoggingService.Current.LogInfo("Enter Execute Command");

            var deviceId = settings.DeviceId!.Trim();

            var device = BluetoothTransport.FindDevice(deviceId);

            if (device == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Device {AnsiMarkupFormatter.EscapeString(deviceId)} has not been discovered."));
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("Wake the device so it advertises, then use 'midi bluetooth list' to confirm the device id."));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            // The endpoint is matched on its instance id, which the transport derives from the
            // device, so a customization can be written before the device has ever connected.
            var instanceId = device.GetNamedString(BluetoothTransport.ResponseEndpointDeviceInstanceId, string.Empty);

            if (string.IsNullOrEmpty(instanceId))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("This device has no endpoint instance id, so it cannot be customized."));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var customProperties = BluetoothTransport.BuildCustomProperties(
                settings.Name, settings.Description, settings.Image, settings.Clear);

            if (!BluetoothTransport.ApplyCustomization(instanceId, customProperties))
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(
                settings.Clear ? "Customization removed." : "Customization applied."));

            AnsiConsole.WriteLine();

            var transportName = device.GetNamedString(BluetoothTransport.ResponseName, string.Empty);

            if (!string.IsNullOrEmpty(transportName))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    $"The name reported by the device is {AnsiMarkupFormatter.EscapeString(transportName)}."));
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
