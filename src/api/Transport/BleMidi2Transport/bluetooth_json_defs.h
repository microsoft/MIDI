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

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_AVAILABLE_DEVICES_RESPONSE_KEY         L"availableDevices"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY                          L"deviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY                        L"name"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SELECTED_PROTOCOL_KEY                  L"selectedProtocol"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_KEY                 L"nativeDataFormat"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY                       L"isConnected"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY                          L"isPaired"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SIGNAL_STRENGTH_KEY                    L"signalStrengthDbm"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY                 L"endpointDeviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_ARGUMENT_DEVICE_ID_KEY         L"deviceId"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_RESULT_HRESULT_KEY             L"hresult"

// devices to connect to automatically, read from the configuration file at service start
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY                      L"devices"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ENABLED_KEY                     L"enabled"

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1                   L"bleMidi1"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP               L"bleMidi2Ump"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_UNKNOWN                 L"unknown"

#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_MIDI1         L"timestampedMidi1ByteStream"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UMP           L"ump"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UNKNOWN       L"unknown"
