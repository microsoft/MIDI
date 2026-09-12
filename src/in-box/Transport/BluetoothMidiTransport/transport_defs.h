// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// ---------------------------------------------------------------------------------------------
// Timeouts for the WinRT Bluetooth calls this transport makes.
//
// These are grouped by what the call is FOR, not by which API it happens to be, because the right
// budget for bringing a link up is nothing like the right budget for sending a packet on one which
// is already up. A GATT call against a device which has gone to sleep or out of range blocks for
// the full Bluetooth timeout, and service shutdown joins the threads making these calls, so every
// one of them is bounded. Waits are taken in slices (see the poll slice below) so that shutdown
// does not have to sit out a whole timeout.
// ---------------------------------------------------------------------------------------------

// Bringing a link up: opening the device object, discovering and opening the GATT service,
// selecting the characteristic, creating the session, and the descriptor write which turns
// notifications on. The radio may have to wait several advertising intervals, and a device which
// demands pairing adds an entire security exchange. Five seconds gave up on devices which were
// about to succeed.
#define MIDI_BLE_CONNECT_OPERATION_TIMEOUT_MS                           12000

// Everything off the data path which is not bringing a link up: reading properties, resolving a
// name, enumerating the radio.
#define MIDI_BLE_GENERAL_OPERATION_TIMEOUT_MS                           5000

// Sending on a link which is already up. A write which takes longer than this has already missed
// its moment musically, so waiting further only queues more behind it.
#define MIDI_BLE_DATA_OPERATION_TIMEOUT_MS                              2000

// Tearing a link down. Shutdown joins these threads and the link is going away regardless, so
// courtesy calls get the shortest wait of anything here.
#define MIDI_BLE_TEARDOWN_OPERATION_TIMEOUT_MS                          1000

// How often a wait in progress re-checks whether the transport is shutting down. This is not a
// timeout: it is the granularity at which any of the timeouts above can be abandoned early.
#define MIDI_BLE_AWAIT_POLL_SLICE_MS                                    200

// the IDs here aren't the full Ids, just the values we start with
// The full Id comes back from the swdevicecreate callback

#define TRANSPORT_LAYER_GUID __uuidof(Midi2BluetoothMidiTransport);

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

// A link which comes up and goes away again this quickly did not fail for range or power reasons.
// Devices which demand security over SMP rather than through a GATT error look exactly like this.
#define MIDI_BLE_UNPAIRED_EARLY_DROP_MS                                 10000

// More than one, so a single unlucky drop is not read as a demand for pairing. A device which
// merely connects unreliably produces the same signal, so this sits above the one or two retries
// such a device typically needs. A link which delivered any message never counts at all, because a
// device refusing an unpaired connection drops before it sends anything.
#define MIDI_BLE_UNPAIRED_EARLY_DROPS_BEFORE_PAIRING_ASSUMED            3

// How often the worker re-examines the remembered devices which are not connected. Advertisements
// are the usual trigger, but a bonded device which is not advertising produces none, so without
// this a device that failed once is never tried again. Deliberately shorter than the retry
// interval above, which is what actually paces the attempts: this only decides how soon after a
// device becomes reachable the next attempt is allowed to happen.
#define MIDI_BLE_CONNECT_SWEEP_INTERVAL_MS                              3000

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


