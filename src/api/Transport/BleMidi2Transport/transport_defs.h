// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// the IDs here aren't the full Ids, just the values we start with
// The full Id comes back from the swdevicecreate callback

#define TRANSPORT_LAYER_GUID __uuidof(Midi2Ble2MidiTransport);

#define TRANSPORT_MANUFACTURER                                          L"Microsoft"
#define TRANSPORT_CODE                                                  L"BLE20"
#define MIDI_NETWORK_ENDPOINT_INSTANCE_ID_HOST_PREFIX                   L"MIDIU_BLE20_H_"
#define MIDI_NETWORK_ENDPOINT_INSTANCE_ID_CLIENT_PREFIX                 L"MIDIU_BLE20_C_"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID                                             L"MIDIU_BLE20_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME                                    L"MIDI 2.0 Bluetooth Low Energy (BLE) Endpoints"

#define LOOPBACK_PARENT_ROOT                                            L"HTREE\\ROOT\\0"
#define TRANSPORT_ENUMERATOR                                            L"MIDISRV"


