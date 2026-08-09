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
#define MIDI_NETWORK_TRANSPORT_ID                                       L"{c95dcd1f-cde3-4c2d-913c-528cb8a4cbe6}" // for the client API which doesn't know about the internal types here

#define TRANSPORT_MANUFACTURER                                          L"Microsoft"
#define TRANSPORT_CODE                                                  L"NET2UDP"

// Endpoint identity is deliberately role-free. A device which connects in both the Host and the
// Client role presents identical identity in both (spec section 12), and the spec expects us to
// recognize that rather than create two unrelated endpoints.
#define MIDI_NETWORK_ENDPOINT_INSTANCE_ID_PREFIX                        L"MIDIU_NET2UDP_"

// How much of the UMP Endpoint Name is kept in the instance id for readability. The hash which
// follows it is what actually provides uniqueness.
#define MIDI_NETWORK_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS                24


#define TRANSPORT_HOST_PARENT_NAME_PREFIX                               L"MIDI 2.0 Network Local Host: "    // TODO: Names should be moved to .rc for localization
#define TRANSPORT_HOST_PARENT_ID_PREFIX                                 L"MIDIU_NET2UDP_HOST_"

#define TRANSPORT_CLIENT_PARENT_ID                                      L"MIDIU_NET2UDP_TRANSPORT"
#define TRANSPORT_CLIENT_PARENT_DEVICE_NAME                             L"MIDI 2.0 Network Remote Hosts"    // TODO: Names should be moved to .rc for localization

#define ULTIMATE_PARENT_ROOT                                            L"HTREE\\ROOT\\0"
#define TRANSPORT_ENUMERATOR                                            L"MIDISRV"


#define DNS_PTR_SERVICE_TYPE                                            L"_midi2._udp.local"
#define MIDI_UDP_PAYLOAD_HEADER                                         0x4D494449                      // "MIDI" in ASCII


#define MIDI_MAX_UMP_WORDS_PER_PACKET                                   64          // spec section 7.1

// Spec section 5.2: "UDP packets should not exceed 1400 bytes". This is our UDP payload, so the
// IP and UDP headers sit on top of it: 1428 total for IPv4 and 1448 for IPv6, both inside a
// standard 1500 byte Ethernet MTU. DontFragment is set on the socket, so a datagram larger than
// the path MTU is dropped outright rather than fragmented.
#define MIDI_NETWORK_MAX_UDP_PAYLOAD_BYTES                              1400
#define MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT                           98          // Spec sections 6.4 - 6.8         
#define MIDI_MAX_UMP_PRODUCT_INSTANCE_ID_BYTE_COUNT                     42          // Spec sections 6.4 - 6.8
// NAK payload length counts the original command header word as well, leaving 254 words of text
#define MIDI_MAX_NAK_MESSAGE_BYTE_COUNT                                 1016        // Spec 6.15 : (254 * sizeof(uint32_t))
#define MIDI_MAX_BYE_MESSAGE_BYTE_COUNT                                 1020        // Spec 6.16 : (255 * sizeof(uint32_t))

#define MIDI_COMMAND_PAYLOAD_LENGTH_NO_PAYLOAD                          0

#define MIDI_NETWORK_COMMAND_RETRANSMIT_INTERVAL_MS                     1000

// How many times we ask for the same missing packets before accepting the loss and moving on.
// Spec 7.2.3: a remote which does not implement retransmit NAKs the request, and we must not
// keep asking. A remote which simply never answers must not be able to wedge the session either.
#define MIDI_NETWORK_MAX_RETRANSMIT_REQUEST_ATTEMPTS                    3

// Spec 6.4: "The Invitation Command should be sent repeatedly, with a reasonable delay between
// Invitation Commands, until a reply is received... If the Client considers the Invitation
// failed, the Client shall terminate the Invitation with a Bye Command with reason 0x80."
// Retries are driven by the connection watchdog tick, so this is that many ticks.
#define MIDI_NETWORK_MAX_INVITATION_ATTEMPTS                            5

// Spec 6.8: Invitation Reply: Pending means the host received the invitation but needs time,
// typically because a person has to approve it on the device. Once we have that reply we stop
// re-inviting and simply wait, so this timeout is scaled to a human walking over to a device
// and clicking accept, not to network round trips.
#define MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_DEFAULT                 120000
#define MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_UPPER_BOUND             600000
#define MIDI_NETWORK_INVITATION_PENDING_TIMEOUT_LOWER_BOUND             1000

// Spec 6.16: "The Bye Command should be sent repeatedly until a Bye Reply Command is received,
// or until a timeout occurs." Only the user-initiated disconnect path does this. Shutdown paths
// send once and move on, because waiting there runs against the service stop timeout and, with
// many sessions, would multiply.
#define MIDI_NETWORK_BYE_MAX_ATTEMPTS                                   3
#define MIDI_NETWORK_BYE_REPLY_TIMEOUT_MILLISECONDS                     500

// Storing a datagram to a socket output stream normally completes immediately. This bound only
// exists so that a stack or remote which never completes the store cannot hold the writer lock,
// and with it a session teardown, forever.
#define MIDI_NETWORK_SEND_TIMEOUT_MILLISECONDS                          2000

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
#define MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_UPPER_BOUND        300000
#define MIDI_NETWORK_DIRECT_CONNECTION_SCAN_INTERVAL_LOWER_BOUND        250

// Connecting to a remote host resolves a name and can stall with no timeout of its own. Client
// startup runs on the shared background worker, so one unreachable host would otherwise hold up
// every other configured client behind it.
#define MIDI_NETWORK_CLIENT_CONNECT_TIMEOUT_MILLISECONDS                5000

#define MIDI_NETWORK_STARTING_OUTBOUND_UMP_QUEUE_CAPACITY               50

// Upper bound on simultaneous remote clients for a single host. Datagram source addresses are
// trivially forged, so without a cap a single sender can make us allocate connections and
// threads without limit. The default is user-configurable, but never above the absolute max:
// each connection currently costs two threads, so this is a real resource decision.
#define MIDI_NETWORK_HOST_MAX_CONNECTIONS_DEFAULT                       64
#define MIDI_NETWORK_HOST_MAX_CONNECTIONS_LOWER_BOUND                   1
#define MIDI_NETWORK_HOST_MAX_CONNECTIONS_ABSOLUTE_MAX                  512

// A connection with no session and no traffic for this long is reclaimed. Remote clients
// normally reconnect from a new ephemeral source port, so without this the connection map grows
// by one object and two threads on every reconnect.
#define MIDI_NETWORK_CONNECTION_IDLE_RECLAIM_MILLISECONDS               30000

#define MIDI_NETWORK_MIDI_CREATE_MIDI1_PORTS_DEFAULT                    false

// header sized plus a command packet header
#define MINIMUM_VALID_UDP_PACKET_SIZE (sizeof(uint32_t) * 2)




enum MidiNetworkConnectionRole
{
    ConnectionWindowsIsHost,
    ConnectionWindowsIsClient,
};
