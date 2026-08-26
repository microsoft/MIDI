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

            if (settings.Clear && (settings.Name != null || settings.Description != null || settings.Image != null))
            {
                return ValidationResult.Error("--clear removes every customization, so it cannot be combined with --name, --description, or --image.");
            }

            if (!settings.Clear && settings.Name == null && settings.Description == null && settings.Image == null)
            {
                return ValidationResult.Error("Nothing to change. Supply --name, --description, or --image, or use --clear to remove the customization.");
            }

            // The service loads images by name from a known folder. A path would either escape
            // that folder or simply fail to load, so it is rejected here rather than at the
            // service, where the error is far less obvious.
            if (!string.IsNullOrEmpty(settings.Image) &&
                (settings.Image.Contains('\\') || settings.Image.Contains('/') || Path.IsPathRooted(settings.Image)))
            {
                return ValidationResult.Error("The image must be a file name only, not a path.");
            }

            return ValidationResult.Success();
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

            var customProperties = new JsonObject();

            if (!settings.Clear)
            {
                // An empty string is meaningful: it clears that one property while leaving the
                // others alone.
                if (settings.Name != null)
                {
                    customProperties.SetNamedValue(BluetoothTransport.CustomNameKey, JsonValue.CreateStringValue(settings.Name.Trim()));
                }

                if (settings.Description != null)
                {
                    customProperties.SetNamedValue(BluetoothTransport.CustomDescriptionKey, JsonValue.CreateStringValue(settings.Description.Trim()));
                }

                if (settings.Image != null)
                {
                    customProperties.SetNamedValue(BluetoothTransport.CustomImageKey, JsonValue.CreateStringValue(settings.Image.Trim()));
                }
            }

            var match = new JsonObject();
            match.SetNamedValue(BluetoothTransport.MatchDeviceInstanceIdKey, JsonValue.CreateStringValue(instanceId));

            var updateEntry = new JsonObject();
            updateEntry.SetNamedValue(BluetoothTransport.MatchKey, match);
            updateEntry.SetNamedValue(BluetoothTransport.CustomPropertiesKey, customProperties);

            var updateArray = new JsonArray { updateEntry };

            var updateObject = new JsonObject();
            updateObject.SetNamedValue(BluetoothTransport.UpdateKey, updateArray);

            if (!BluetoothTransport.SendUpdate(updateObject))
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

            if (!BluetoothConfigFile.TryUpsertCustomization(instanceId, ToSystemTextJson(customProperties), out var error))
            {
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(AnsiMarkupFormatter.EscapeString(error ?? "The customization could not be saved.")));
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage("This customization will be lost when the service restarts."));
            }

            return (int)MidiConsoleReturnCode.Success;
        }

        // The config file is edited with System.Text.Json so the rest of it survives untouched,
        // but the service command uses Windows.Data.Json.
        private static System.Text.Json.Nodes.JsonObject ToSystemTextJson(JsonObject source)
        {
            var result = new System.Text.Json.Nodes.JsonObject();

            foreach (var pair in source)
            {
                result[pair.Key] = pair.Value.GetString();
            }

            return result;
        }
    }
}
