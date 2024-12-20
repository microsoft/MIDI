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

#define TRANSPORT_MANUFACTURER L"Microsoft"
#define TRANSPORT_CODE L"UDP"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID L"MIDIU_UDP_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Network (UDP) Devices"


#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"

#define TRANSPORT_ENUMERATOR L"MIDISRV"


#define DNS_PTR_SERVICE_TYPE        L"_midi2._udp.local"
#define MIDI_UDP_PAYLOAD_HEADER     0x4D494449              // "MIDI" in ASCII


#define MAX_UMP_ENDPOINT_NAME_LENGTH          98
#define MAX_UMP_PRODUCT_INSTANCE_ID_LENGTH    42


// JSON keys. Can move to json_defs when in-box

// Network MIDI 2.0

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_KEY                                 L"host"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_ENTRY_IDENTIFIER_KEY                     L"entryIdentifier"    // GUID or other string for the entry in the config file
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_INTERFACE_KEY                            L"networkInterface"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_KEY                     L"networkProtocol"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP               L"udp"                  // UDP is only protocol currently supported

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_MDNS_ADVERTISE_KEY                       L"advertise"            // boolean


#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY                L"serviceInstanceName"     // just the first part (before the . ) of the host instance name. Defaults to machine name

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PORT_KEY                         L"port"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_NETWORK_PORT_VALUE_AUTO                  L"auto"

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_KEY                     L"connectionPolicyIpv4"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_ANY    L"allowAny"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_LIST   L"allowList"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_ALLOW_IPV4_VALUE_RANGE  L"allowRange"

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CONNECTION_POLICY_IPV4_ADDRESSES_KEY     L"addresses"            // list, range . We keep this simple. ANything more complex should be done with the firewall

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_KEY                  L"authentication"       // password, user, none
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE           L"none"
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_PASSWORD       L"password"             // global password
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_USER           L"user"                 // user and password

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_GLOBAL_PASSWORD_KEY  L"globalPassword"       // credential key to entry in vault
#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_HOST_AUTHENTICATION_USER_AUTH_KEY        L"userAuth"             // credential key to user/password entry in vault


#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY             L"productInstanceId"

#define MIDI_CONFIG_JSON_ENDPOINT_NETWORK_MIDI_CLIENT_KEY                               L"client"