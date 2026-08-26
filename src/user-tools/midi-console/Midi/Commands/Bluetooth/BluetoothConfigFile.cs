// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Devices.Midi2.Tools.Shared.Config;

namespace Microsoft.Midi.ConsoleApp
{
    // The service never writes the configuration file; user tools own it. Without this, a
    // connection is remembered only until the service restarts.
    internal static class BluetoothConfigFile
    {
        private const string TransportPluginSettingsKey = "endpointTransportPluginSettings";
        private const string DevicesKey = "devices";
        private const string DeviceIdKey = "deviceId";
        private const string EnabledKey = "enabled";
        private const string UpdateKey = "update";
        private const string PeripheralKey = "peripheral";
        private const string ProtocolKey = "protocol";

        // Section keys are written by the service as braced upper case, and a differently cased
        // key would silently become a second section the service never reads.
        private static readonly string TransportSectionKey = "{" + BluetoothTransport.TransportId.ToString().ToUpperInvariant() + "}";

        internal static string? FilePath => ConfigFileHelper.GetConfigFileFullPath();

        internal static bool TryAddDevice(string deviceId, out string? error)
        {
            return TryUpdateDevices(
                devices =>
                {
                    foreach (var existing in devices)
                    {
                        if (string.Equals(existing?[DeviceIdKey]?.GetValue<string>(), deviceId, StringComparison.OrdinalIgnoreCase))
                        {
                            existing![EnabledKey] = true;
                            return true;
                        }
                    }

                    devices.Add(new JsonObject
                    {
                        [DeviceIdKey] = deviceId,
                        [EnabledKey] = true,
                    });

                    return true;
                },
                out error);
        }

        internal static bool TryRemoveDevice(string deviceId, out string? error)
        {
            return TryUpdateDevices(
                devices =>
                {
                    for (var i = devices.Count - 1; i >= 0; i--)
                    {
                        if (string.Equals(devices[i]?[DeviceIdKey]?.GetValue<string>(), deviceId, StringComparison.OrdinalIgnoreCase))
                        {
                            devices.RemoveAt(i);
                        }
                    }

                    return true;
                },
                out error);
        }

        // Publishing this PC is a machine-wide setting, so it lives beside the device list rather
        // than in the update section.
        internal static bool TrySetPeripheral(bool enabled, bool useMidi2, out string? error)
        {
            return TryUpdateTransportSection(
                transportSection =>
                {
                    if (transportSection[PeripheralKey] is not JsonObject peripheral)
                    {
                        peripheral = new JsonObject();
                        transportSection[PeripheralKey] = peripheral;
                    }

                    peripheral[EnabledKey] = enabled;

                    if (enabled)
                    {
                        peripheral[ProtocolKey] = useMidi2 ? BluetoothTransport.ProtocolValueMidi2Ump : BluetoothTransport.ProtocolValueMidi1;
                    }

                    return true;
                },
                out error);
        }

        // The service applies the update section on every start, so a customization written here
        // is what gives a renamed endpoint the same name after a reboot.
        internal static bool TryUpsertCustomization(string endpointDeviceInstanceId, JsonObject customProperties, out string? error)
        {
            return TryUpdateTransportSection(
                transportSection =>
                {
                    if (transportSection[UpdateKey] is not JsonArray updates)
                    {
                        updates = new JsonArray();
                        transportSection[UpdateKey] = updates;
                    }

                    for (var i = updates.Count - 1; i >= 0; i--)
                    {
                        var match = updates[i]?[BluetoothTransport.MatchKey] as JsonObject;

                        if (string.Equals(
                            match?[BluetoothTransport.MatchDeviceInstanceIdKey]?.GetValue<string>(),
                            endpointDeviceInstanceId,
                            StringComparison.OrdinalIgnoreCase))
                        {
                            updates.RemoveAt(i);
                        }
                    }

                    // Only written when something is left to say, so clearing every property
                    // removes the entry instead of leaving an empty one behind.
                    if (customProperties.Count > 0)
                    {
                        updates.Add(new JsonObject
                        {
                            [BluetoothTransport.MatchKey] = new JsonObject
                            {
                                [BluetoothTransport.MatchDeviceInstanceIdKey] = endpointDeviceInstanceId,
                            },
                            [BluetoothTransport.CustomPropertiesKey] = customProperties.DeepClone(),
                        });
                    }

                    return true;
                },
                out error);
        }

        private static bool TryUpdateDevices(Func<JsonArray, bool> update, out string? error)
        {
            return TryUpdateTransportSection(
                transportSection =>
                {
                    if (transportSection[DevicesKey] is not JsonArray devices)
                    {
                        devices = new JsonArray();
                        transportSection[DevicesKey] = devices;
                    }

                    return update(devices);
                },
                out error);
        }

        private static bool TryUpdateTransportSection(Func<JsonObject, bool> update, out string? error)
        {
            error = null;

            var path = FilePath;

            if (string.IsNullOrEmpty(path))
            {
                error = "No configuration file is registered, so this device could not be saved.";
                return false;
            }

            try
            {
                JsonObject root;

                if (File.Exists(path))
                {
                    // Parsed as a DOM rather than deserialized, so every other transport's
                    // settings survive untouched.
                    root = JsonNode.Parse(File.ReadAllText(path)) as JsonObject
                        ?? throw new InvalidDataException("The configuration file is not a JSON object.");
                }
                else
                {
                    root = new JsonObject();
                }

                if (root[TransportPluginSettingsKey] is not JsonObject pluginSettings)
                {
                    pluginSettings = new JsonObject();
                    root[TransportPluginSettingsKey] = pluginSettings;
                }

                if (pluginSettings[TransportSectionKey] is not JsonObject transportSection)
                {
                    transportSection = new JsonObject();
                    pluginSettings[TransportSectionKey] = transportSection;
                }

                if (!update(transportSection))
                {
                    return false;
                }

                File.WriteAllText(path, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));

                return true;
            }
            catch (UnauthorizedAccessException)
            {
                error = $"No permission to write the configuration file at {path}.";
                return false;
            }
            catch (Exception ex)
            {
                error = $"The configuration file at {path} could not be updated. {ex.Message}";
                return false;
            }
        }
    }
}
