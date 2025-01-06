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



#define MIDI_NETWORK_STARTING_OUTBOUND_UMP_QUEUE_CAPACITY               50

// JSON keys. Can move to json_defs when in-box

// Network MIDI 2.0

#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY                                 L"hosts"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY                               L"clients"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_KEY                    L"transportSettings"


#define MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY                       L"maxForwardErrorCorrectionCommandPackets"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY                L"maxRetransmitBufferCommandPackets"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY                L"outboundPingInterval"


#define MIDI_CONFIG_JSON_NETWORK_MIDI_INTERFACE_KEY                             L"networkInterface"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY                      L"networkProtocol"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP                L"udp"                      // UDP is only protocol currently supported

#define MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY                        L"advertise"                // boolean
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY                               L"enabled"                  // boolean


#define MIDI_CONFIG_JSON_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY                 L"serviceInstanceName"      // just the first part (before the . ) of the host instance name. Defaults to machine name

#define MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_KEY                          L"port"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO                   L"auto"


#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_KEY                     L"connectionPolicyIpv4"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY    L"allowAny"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_LIST   L"allowList"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_RANGE  L"allowRange"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_POLICY_IPV4_ADDRESSES_KEY      L"addresses"             // list, range . We keep this simple. Anything more complex should be done with the firewall

#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_KEY                   L"authentication"        // password, user, none
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE            L"none"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_PASSWORD        L"password"              // global password
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_USER            L"user"                  // user and password

#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_GLOBAL_PASSWORD_KEY   L"globalPassword"        // credential key to entry in vault
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_USER_AUTH_KEY         L"userAuth"              // credential key to user/password entry in vault


#define MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY              L"productInstanceId"


#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID                           L"id"                    // Windows ID like:  DnsSd#kb7C5D0A_1._midi2._udp.local#0
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_SERVICE_INSTANCE             L"serviceInstance"       // Like kb7C5D0A_1 or bomeboxdin-8q6d2z-1
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_IP                           L"ipAddress"             // Like 192.168.1.253 (port is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME                    L"hostName"              // Like kissbox or BomeBox.local (port is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT                         L"port"                  // Like 5004 (ip address or host name is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME            L"umpEndpointName"       // Like UMP2TR @253 Port 1 or BomeBox
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_PID             L"umpProductInstanceId"  // Like kb7C5D0A_1 or CC851C0080257A96


enum MidiNetworkConnectionRole
{
    ConnectionWindowsIsHost,
    ConnectionWindowsIsClient,
};
