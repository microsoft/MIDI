// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_LIST_AVAILABLE_DEVICES         L"listAvailableDevices"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_CONNECT_DEVICE                 L"connectDevice"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_DISCONNECT_DEVICE              L"disconnectDevice"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_START_PERIPHERAL               L"startPeripheral"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_STOP_PERIPHERAL                L"stopPeripheral"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PERIPHERAL_STATUS          L"getPeripheralStatus"

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_AVAILABLE_DEVICES_RESPONSE_KEY         L"availableDevices"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY                          L"deviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY                        L"name"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SELECTED_PROTOCOL_KEY                  L"selectedProtocol"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_KEY                 L"nativeDataFormat"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY                       L"isConnected"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY                          L"isPaired"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SIGNAL_STRENGTH_KEY                    L"signalStrengthDbm"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY                 L"endpointDeviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_INSTANCE_ID_KEY        L"endpointDeviceInstanceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_KEY                 L"lastConnectError"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_HRESULT_KEY         L"lastConnectErrorHresult"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY                  L"messagesReceived"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY                      L"messagesSent"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEND_ERROR_HRESULT_KEY            L"lastSendErrorHresult"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PRESENT_KEY                         L"isPresent"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEEN_AGO_MS_KEY                   L"lastSeenAgoMilliseconds"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_ENDPOINT_KEY                       L"hasEndpoint"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_KNOWN_KEY                           L"isKnown"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY         L"deviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_RESULT_HRESULT_KEY             L"hresult"

// devices to connect to automatically, read from the configuration file at service start
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY                      L"devices"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ENABLED_KEY                     L"enabled"

// this PC published as a BLE MIDI peripheral, in both the configuration file and the responses
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY                         L"peripheral"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ENABLED_KEY                 L"enabled"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY                L"protocol"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_IS_RUNNING_KEY              L"isRunning"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ADVERTISED_NAME_KEY         L"advertisedName"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_CLIENT_COUNT_KEY            L"subscribedClientCount"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY                            L"bluetoothAddress"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_TYPE_KEY                       L"bluetoothAddressType"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_BLUETOOTH_DEVICE_ID_KEY                L"bluetoothDeviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_GENERIC_NAME_KEY                   L"hasGenericName"

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1                   L"bleMidi1"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP               L"bleMidi2Ump"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_UNKNOWN                 L"unknown"

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_MIDI1         L"timestampedMidi1ByteStream"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UMP           L"ump"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UNKNOWN       L"unknown"

// Which connection parameters to ask the radio for when connecting out to a device. Windows
// offers only these presets, so this exists mainly to measure what each one actually does.
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_KEY              L"connectionParameters"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_DEFAULT    L"systemDefault"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_THROUGHPUT L"throughputOptimized"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_BALANCED   L"balanced"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_POWER      L"powerOptimized"

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_SET_CONNECTION_PARAMETERS      L"setConnectionParameters"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MIN_INTERVAL_MS_KEY                    L"minConnectionIntervalMilliseconds"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MAX_INTERVAL_MS_KEY                    L"maxConnectionIntervalMilliseconds"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INTERVAL_MS_KEY                        L"connectionIntervalMilliseconds"
