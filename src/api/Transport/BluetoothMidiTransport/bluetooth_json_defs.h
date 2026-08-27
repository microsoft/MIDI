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

// approving a Central which has subscribed to this PC's peripheral
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_GET_PENDING_CLIENTS            L"getPendingPeripheralClients"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_APPROVE_CLIENT                 L"approvePeripheralClient"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_DENY_CLIENT                    L"denyPeripheralClient"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_COMMAND_FORGET_CLIENT                  L"forgetPeripheralClient"

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
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_CODE_KEY            L"lastConnectErrorCode"
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

// False when the address rotates, so a caller knows not to offer "always" for this device
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_REMEMBERABLE_KEY                    L"isRememberable"

// Whether a Central which subscribes is let straight through. WinRT cannot refuse a GATT
// subscription, so requiring approval gates the MIDI endpoint and the data path, not the link.
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_KEY                      L"clientPolicy"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_VALUE_ALLOW_ANY          L"allowAny"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL   L"requireApproval"

// remembered decisions, persisted in the configuration file under the peripheral object
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ALLOWED_CLIENTS_KEY                    L"allowedClients"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DENIED_CLIENTS_KEY                     L"deniedClients"

// getPendingPeripheralClients response, which the setup app polls
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PENDING_CLIENTS_RESPONSE_KEY           L"pendingPeripheralClients"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_REQUEST_TIME_KEY                       L"requestTime"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_KEY                    L"decision"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_VALUE_PENDING          L"pending"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_VALUE_ALLOWED          L"allowed"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_DECISION_VALUE_DENIED           L"denied"

// Set on an approve or deny response when the decision has to be written to the configuration
// file by the caller to survive a restart. The service never writes that file itself.
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERSIST_REQUIRED_KEY                   L"persistRequired"

// Only "always" is written to the configuration file. "once" settles the waiting client and
// nothing else, and "untilRestart" is a memory-only decision by definition.
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_KEY                     L"scope"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_VALUE_ONCE              L"once"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_VALUE_UNTIL_RESTART     L"untilRestart"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_VALUE_ALWAYS            L"always"

// what the radio on this machine can do, so a machine with no Bluetooth explains itself
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_KEY                              L"radio"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_PRESENT_KEY                      L"isPresent"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_LOW_ENERGY_KEY                   L"isLowEnergySupported"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_CENTRAL_ROLE_KEY                 L"isCentralRoleSupported"
#define MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_PERIPHERAL_ROLE_KEY              L"isPeripheralRoleSupported"

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
