// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.Data.Json;
using Windows.Devices.Midi2.ServiceConfig;

namespace Microsoft.Midi.ConsoleApp
{
    internal static class BluetoothTransport
    {
        // Midi2.Ble2MidiTransport.dll, registered as Midi2BluetoothMidiTransport
        internal static readonly Guid TransportId = new Guid("5dc87270-f318-4838-a4f9-6aadc63e925f");

        internal const string VerbListAvailableDevices = "listAvailableDevices";
        internal const string VerbConnectDevice = "connectDevice";
        internal const string VerbDisconnectDevice = "disconnectDevice";
        internal const string VerbStartPeripheral = "startPeripheral";
        internal const string VerbStopPeripheral = "stopPeripheral";
        internal const string VerbGetPeripheralStatus = "getPeripheralStatus";
        internal const string VerbSetConnectionParameters = "setConnectionParameters";

        internal const string ArgumentConnectionParameters = "connectionParameters";
        internal const string ResponseMinIntervalMilliseconds = "minConnectionIntervalMilliseconds";
        internal const string ResponseMaxIntervalMilliseconds = "maxConnectionIntervalMilliseconds";
        internal const string ResponseConnectionIntervalMilliseconds = "connectionIntervalMilliseconds";

        // Short words for the command line, mapped to what the transport expects.
        internal static string? ResolveConnectionParameterPreference(string? value)
        {
            return (value ?? string.Empty).ToLowerInvariant() switch
            {
                "system" or "systemdefault" or "default" or "none" => "systemDefault",
                "throughput" or "throughputoptimized" or "low" => "throughputOptimized",
                "balanced" => "balanced",
                "power" or "poweroptimized" => "powerOptimized",
                _ => null,
            };
        }

        internal static JsonObject? SendConnectionParametersCommand(string preference)
        {
            var command = new MidiServiceTransportCommand(TransportId, VerbSetConnectionParameters);

            command.Arguments[ArgumentConnectionParameters] = preference;

            return SendCommand(command, VerbSetConnectionParameters);
        }

        internal const string ArgumentDeviceId = "deviceId";
        internal const string ArgumentProtocol = "protocol";

        internal const string ProtocolValueMidi1 = "bleMidi1";
        internal const string ProtocolValueMidi2Ump = "bleMidi2Ump";

        internal const string ResponseAvailableDevices = "availableDevices";
        internal const string ResponseDeviceId = "deviceId";
        internal const string ResponseName = "name";
        internal const string ResponseEndpointDeviceId = "endpointDeviceId";
        internal const string ResponseEndpointDeviceInstanceId = "endpointDeviceInstanceId";
        internal const string ResponseIsConnected = "isConnected";
        internal const string ResponseMessagesReceived = "messagesReceived";
        internal const string ResponseMessagesSent = "messagesSent";
        internal const string ResponsePeripheral = "peripheral";
        internal const string ResponsePeripheralIsRunning = "isRunning";
        internal const string ResponsePeripheralProtocol = "protocol";
        internal const string ResponsePeripheralAdvertisedName = "advertisedName";
        internal const string ResponsePeripheralClientCount = "subscribedClientCount";
        internal const string ResponseIsPaired = "isPaired";
        internal const string ResponseBluetoothAddress = "bluetoothAddress";
        internal const string ResponseBluetoothAddressType = "bluetoothAddressType";
        internal const string ResponseHasGenericName = "hasGenericName";

        // These are the shared endpoint customization keys, not Bluetooth-specific ones. Every
        // transport which supports customization reads the same shape.
        internal const string UpdateKey = "update";
        internal const string MatchKey = "match";
        internal const string MatchDeviceInstanceIdKey = "endpointDeviceInstanceId";
        internal const string CustomPropertiesKey = "customProperties";
        internal const string CustomNameKey = "name";
        internal const string CustomDescriptionKey = "description";
        internal const string CustomImageKey = "image";

        internal static JsonObject? SendPeripheralCommand(string verb, bool useMidi2)
        {
            var command = new MidiServiceTransportCommand(TransportId, verb);

            command.Arguments[ArgumentProtocol] = useMidi2 ? ProtocolValueMidi2Ump : ProtocolValueMidi1;

            return SendCommand(command, verb);
        }

        internal static JsonObject? SendCommand(string verb, string? deviceId = null)
        {
            var command = new MidiServiceTransportCommand(TransportId, verb);

            if (!string.IsNullOrEmpty(deviceId))
            {
                command.Arguments[ArgumentDeviceId] = deviceId;
            }

            return SendCommand(command, verb);
        }

        private static JsonObject? SendCommand(MidiServiceTransportCommand command, string verb)
        {
            var response = MidiServiceTransportPluginConfigManager.SendCommand(command);

            if (response.Status != MidiServiceConfigResponseStatus.Success)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(
                    $"The Bluetooth MIDI transport rejected '{verb}'. Status {response.Status}, code {response.ServiceErrorCode}."));

                if (!string.IsNullOrEmpty(response.ServiceErrorMessage))
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(response.ServiceErrorMessage));
                }

                // The transport not being loaded is by far the most common cause, and the error
                // alone does not say so.
                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "If this transport is not installed, it will not appear in 'midi enumerate transport-plugins'."));

                return null;
            }

            return response.ResponseJson;
        }

        // Discovery is the only place a device id can be resolved to the instance id a
        // customization has to match on.
        internal static JsonObject? FindDevice(string deviceId)
        {
            var responseJson = SendCommand(VerbListAvailableDevices);

            if (responseJson == null || !responseJson.ContainsKey(ResponseAvailableDevices))
            {
                return null;
            }

            foreach (var entry in responseJson.GetNamedArray(ResponseAvailableDevices))
            {
                var device = entry.GetObject();

                if (string.Equals(device.GetNamedString(ResponseDeviceId, string.Empty), deviceId, StringComparison.OrdinalIgnoreCase))
                {
                    return device;
                }
            }

            return null;
        }

        internal static bool SendUpdate(JsonObject updateObject)
        {
            var response = MidiServiceTransportPluginConfigManager.SendUpdate(TransportId, updateObject);

            if (response.Status != MidiServiceConfigResponseStatus.Success)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(
                    $"The Bluetooth MIDI transport rejected the update. Status {response.Status}, code {response.ServiceErrorCode}."));

                if (!string.IsNullOrEmpty(response.ServiceErrorMessage))
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(response.ServiceErrorMessage));
                }

                return false;
            }

            return true;
        }
    }
}
