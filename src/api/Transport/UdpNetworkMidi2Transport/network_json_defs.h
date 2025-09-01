// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// JSON keys. Can move to common json_defs when in-box

// Network MIDI 2.0

#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY                                 L"hosts"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY                               L"clients"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_TRANSPORT_SETTINGS_KEY                    L"transportSettings"


#define MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_FEC_PACKETS_KEY                       L"maxForwardErrorCorrectionCommandPackets"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_RETRANSMIT_BUFFER_SIZE_KEY                L"maxRetransmitBufferCommandPackets"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_OUTBOUND_PING_INTERVAL_KEY                L"outboundPingInterval"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_DIRECT_CONNECTION_SCAN_INTERVAL_KEY       L"directConnectionScanInterval"


#define MIDI_CONFIG_JSON_NETWORK_MIDI_INTERFACE_KEY                             L"networkInterface"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_KEY                      L"networkProtocol"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_NETWORK_PROTOCOL_VALUE_UDP                L"udp"                      // UDP is only protocol currently supported

#define MIDI_CONFIG_JSON_NETWORK_MIDI_MDNS_ADVERTISE_KEY                        L"advertise"                // boolean
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY                               L"enabled"                  // boolean

#define MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY                    L"createMidi1Ports"         // boolean - set to true to enable creating WinMM/WinRT 1.0 ports


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




#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY                   L"match"                 // object which contains match criteria
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID_KEY                       L"id"                    // Windows ID like:  DnsSd#kb7C5D0A_1._midi2._udp.local#0
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_SERVICE_INSTANCE_KEY         L"serviceInstance"       // Like kb7C5D0A_1 or bomeboxdin-8q6d2z-1
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME_OR_IP_ADDRESS_KEY  L"directHostNameOrIP"    // Like 192.168.1.253 or bomebox.local (port is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT_KEY                     L"directPort"            // Like 5004 (ip address or host name is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME_KEY        L"umpEndpointName"       // Like UMP2TR @253 Port 1 or BomeBox
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_PID_KEY         L"umpProductInstanceId"  // Like kb7C5D0A_1 or CC851C0080257A96



#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST                       L"startHost"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST                        L"stopHost"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER       L"entryIdentifier"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT                L"disconnectClient"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT                   L"connectDirect"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS              L"remoteAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT                 L"remotePort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER     L"entryIdentifier"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME           L"umpEndpointName"



#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS                L"enumerateClients"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CLIENTS_ARRAY_KEY       MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY           L"entryIdentifier"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_MDNS_MATCH_ID_KEY       L"mdnsMatchId"
//#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_ENABLED_KEY          MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY 
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_SESSION_ACTIVE_KEY   L"sessionActive"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_ADDRESS_KEY      L"remoteAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_PORT_KEY         L"remotePort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_ADDRESS_KEY       L"localAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_PORT_KEY          L"localPort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_UMP_ENDPOINT_ID_KEY     L"endpointDeviceId"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CREATE_MIDI1_PORTS_KEY  MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY



#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_HOSTS                  L"enumerateHosts"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HOSTS_ARRAY_KEY           MIDI_CONFIG_JSON_NETWORK_MIDI_HOSTS_KEY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONFIG_ID_KEY             L"entryIdentifier"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_IS_ENABLED_KEY            MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY 
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_HAS_STARTED_KEY           L"hasStarted"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_ADDRESS_KEY        L"actualAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_ACTUAL_PORT_KEY           L"actualPort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_NAME_KEY                  MIDI_CONFIG_JSON_ENDPOINT_COMMON_NAME_PROPERTY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_PRODUCT_INSTANCE_ID_KEY   MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CREATE_MIDI1_PORTS_KEY    MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_SERVICE_INSTANCE_NAME_KEY MIDI_CONFIG_JSON_NETWORK_MIDI_SERVICE_INSTANCE_NAME_KEY



