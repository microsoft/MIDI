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

#define TRANSPORT_LAYER_GUID __uuidof(Midi2NetworkMidiTransport);

#define TRANSPORT_MANUFACTURER                                          L"Microsoft"
#define TRANSPORT_CODE                                                  L"NET2UDP"
#define MIDI_NETWORK_ENDPOINT_INSTANCE_ID_HOST_PREFIX                   L"MIDIU_NET2UDP_H_"
#define MIDI_NETWORK_ENDPOINT_INSTANCE_ID_CLIENT_PREFIX                 L"MIDIU_NET2UDP_C_"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID                                             L"MIDIU_NET2UDP_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME                                    L"MIDI 2.0 Network (UDP) Endpoints"

#define LOOPBACK_PARENT_ROOT                                            L"HTREE\\ROOT\\0"
#define TRANSPORT_ENUMERATOR                                            L"MIDISRV"


#define DNS_PTR_SERVICE_TYPE                                            L"_midi2._udp.local"
#define MIDI_UDP_PAYLOAD_HEADER                                         0x4D494449                      // "MIDI" in ASCII


#define MIDI_MAX_UMP_WORDS_PER_PACKET                                   64          // spec section 7.1
#define MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT                           98          // Spec sections 6.4 - 6.8         
#define MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT                     42          // Spec sections 6.4 - 6.8
#define MIDI_MAX_NAK_MESSAGE_BYTE_COUNT                                 1020        // Spec 6.15 : (255 * sizeof(uint32_t))
#define MIDI_MAX_BYE_MESSAGE_BYTE_COUNT                                 1020        // Spec 6.16 : (255 * sizeof(uint32_t))

#define MIDI_COMMAND_PAYLOAD_LENGTH_NO_PAYLOAD                          0

#define MIDI_NETWORK_COMMAND_RETRANSMIT_INTERVAL_MS                     1000

#define MIDI_NETWORK_FEC_PACKET_COUNT_DEFAULT                           2
#define MIDI_NETWORK_FEC_PACKET_COUNT_UPPER_BOUND                       10
#define MIDI_NETWORK_FEC_PACKET_COUNT_LOWER_BOUND                       0

#define MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_DEFAULT             50
#define MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_UPPER_BOUND         1000
#define MIDI_NETWORK_RETRANSMIT_BUFFER_PACKET_COUNT_LOWER_BOUND         0

#define MIDI_NETWORK_OUTBOUND_PING_INTERVAL_DEFAULT                     2000
#define MIDI_NETWORK_OUTBOUND_PING_INTERVAL_UPPER_BOUND                 120000
#define MIDI_NETWORK_OUTBOUND_PING_INTERVAL_LOWER_BOUND                 250

#define MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_DEFAULT            20000       // how frequently we try to open a remote IP and port

#define MIDI_NETWORK_STARTING_OUTBOUND_UMP_QUEUE_CAPACITY               50

#define MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT                    false

// header sized plus a command packet header
#define MINIMUM_VALID_UDP_PACKET_SIZE (sizeof(uint32_t) * 2)




enum MidiNetworkConnectionRole
{
    ConnectionWindowsIsHost,
    ConnectionWindowsIsClient,
};
