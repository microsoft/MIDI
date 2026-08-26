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

// How recently a device must have advertised to be considered still in range. A connected device
// stops advertising, so presence for those comes from the link instead.
#define MIDI_BLE_DEVICE_PRESENT_WITHIN_MS                               15000

// Minimum gap between connection attempts for one remembered device. A failed attempt against a
// sleeping device costs a GATT timeout, so retries are deliberately unhurried.
#define MIDI_BLE_CONNECT_RETRY_INTERVAL_MS                              10000

// A name which is only in the scan response may not be known to the Bluetooth stack yet when the
// first advertisement arrives. A device is not listed or connectable until its name is known or
// these attempts are exhausted, so this is kept short.
#define MIDI_BLE_NAME_RESOLUTION_RETRY_INTERVAL_MS                      2000
#define MIDI_BLE_NAME_RESOLUTION_MAX_ATTEMPTS                           3

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID                                             L"MIDIU_BLEMIDI_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME                                    L"Bluetooth Low Energy MIDI Endpoints"

#define MIDI_BLE_MIDI1_ENDPOINT_DESCRIPTION                             L"Bluetooth Low Energy MIDI 1.0 endpoint"
#define MIDI_BLE_MIDI2_ENDPOINT_DESCRIPTION                             L"Bluetooth Low Energy MIDI 2.0 endpoint (Universal MIDI Packet)"

// A remote Central connected to this PC while it is published as a BLE MIDI Peripheral. The
// endpoint represents the remote device, the same way a Network MIDI 2.0 host endpoint does, so
// it carries that device's name and is distinguished from an endpoint for a device this PC
// connected out to.
#define MIDI_BLE_PERIPHERAL_DEVICE_ID                                   L"PERIPHERAL"
#define MIDI_BLE_PERIPHERAL_ENDPOINT_INSTANCE_ID_PREFIX                 MIDI_BLE_ENDPOINT_INSTANCE_ID_PREFIX L"PERIPHERAL_"

// A device which is not bonded rotates its Bluetooth address every few minutes, so keying on the
// address would mint a new endpoint every rotation. All unpaired devices therefore share one
// reusable node, which caps the clutter at a single entry.
#define MIDI_BLE_PERIPHERAL_UNPAIRED_ENDPOINT_INSTANCE_ID               MIDI_BLE_PERIPHERAL_ENDPOINT_INSTANCE_ID_PREFIX L"UNPAIRED"
#define MIDI_BLE_PERIPHERAL_UNKNOWN_CLIENT_NAME                         L"Bluetooth MIDI Client"
#define MIDI_BLE_PERIPHERAL_MIDI1_ENDPOINT_DESCRIPTION                  L"Bluetooth Low Energy MIDI 1.0 device connected to this PC"
#define MIDI_BLE_PERIPHERAL_MIDI2_ENDPOINT_DESCRIPTION                  L"Bluetooth Low Energy MIDI 2.0 device (Universal MIDI Packet) connected to this PC"

#define LOOPBACK_PARENT_ROOT                                            L"HTREE\\ROOT\\0"
#define TRANSPORT_ENUMERATOR                                            L"MIDISRV"


