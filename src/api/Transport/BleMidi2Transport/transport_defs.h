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
#define TRANSPORT_CODE                                                  L"BLEMIDI"

#define MIDI_BLE_ENDPOINT_INSTANCE_ID_PREFIX                            L"MIDIU_BLEMIDI_"
#define MIDI_BLE_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS                    24

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID                                             L"MIDIU_BLEMIDI_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME                                    L"Bluetooth Low Energy MIDI Endpoints"

#define MIDI_BLE_MIDI1_ENDPOINT_DESCRIPTION                             L"Bluetooth Low Energy MIDI 1.0 endpoint"
#define MIDI_BLE_MIDI2_ENDPOINT_DESCRIPTION                             L"Bluetooth Low Energy MIDI 2.0 endpoint (Universal MIDI Packet)"

#define LOOPBACK_PARENT_ROOT                                            L"HTREE\\ROOT\\0"
#define TRANSPORT_ENUMERATOR                                            L"MIDISRV"


