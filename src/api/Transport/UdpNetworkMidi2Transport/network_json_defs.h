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
#define MIDI_CONFIG_JSON_NETWORK_MIDI_INVITATION_PENDING_TIMEOUT_KEY            L"invitationPendingTimeout"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_MAX_HOST_CONNECTIONS_KEY                  L"maxHostConnections"
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


// Remote client approval. The old connectionPolicyIpv4 list and range options were never
// really implemented, and an address is the wrong thing to approve anyway: a device is
// identified by its UMP Endpoint Name and Product Instance Id, and its address moves.
// Older keys are simply not read, so a configuration containing them still loads.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_KEY                  L"remoteClientPolicy"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY      L"allowAny"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL L"requireApproval"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_ALLOWED_CLIENTS_KEY                       L"allowedClients"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_DENIED_CLIENTS_KEY                        L"deniedClients"

// identity of a remote client in the allow and deny lists, and in approval commands
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_NAME_KEY                  L"umpEndpointName"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_PRODUCT_INSTANCE_ID_KEY   L"productInstanceId"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_KEY                   L"authentication"        // password, user, none
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_NONE            L"none"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_PASSWORD        L"password"              // global password
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_VALUE_USER            L"user"                  // user and password

#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_GLOBAL_PASSWORD_KEY   L"globalPassword"        // credential key to entry in vault
#define MIDI_CONFIG_JSON_NETWORK_MIDI_HOST_AUTHENTICATION_USER_AUTH_KEY         L"userAuth"              // credential key to user/password entry in vault


#define MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY              L"productInstanceId"

// same key name, but in the transportSettings section. Machine-wide identity used by every
// host and by this PC's client identity when they do not specify their own.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_MACHINE_PRODUCT_INSTANCE_ID_KEY           MIDI_CONFIG_JSON_NETWORK_MIDI_PRODUCT_INSTANCE_ID_PROPERTY




#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_OBJECT_KEY                   L"match"                 // object which contains match criteria
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_ID_KEY                       L"id"                    // Windows ID like:  DnsSd#kb7C5D0A_1._midi2._udp.local#0
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_SERVICE_INSTANCE_KEY         L"serviceInstance"       // Like kb7C5D0A_1 or bomeboxdin-8q6d2z-1
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_HOST_NAME_OR_IP_ADDRESS_KEY  L"directHostNameOrIP"    // Like 192.168.1.253 or bomebox.local (port is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_PORT_KEY                     L"directPort"            // Like 5004 (ip address or host name is also required)
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_NAME_KEY        L"umpEndpointName"       // Like UMP2TR @253 Port 1 or BomeBox
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_MATCH_UMP_ENDPOINT_PID_KEY         L"umpProductInstanceId"  // Like kb7C5D0A_1 or CC851C0080257A96



#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_START_HOST                       L"startHost"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_STOP_HOST                        L"stopHost"

// Stops the host and forgets it entirely, which is what releases its service instance name.
// stopHost keeps the entry, so a stopped host still holds its name and can be started again.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_REMOVE_HOST                      L"removeHost"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_HOST_ENTRY_IDENTIFIER       L"entryIdentifier"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DISCONNECT_CLIENT                L"disconnectClient"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_CONNECT_DIRECT                   L"connectDirect"

// Approval of a remote client which is waiting in the pending state, or pre-approval of one
// which has not connected yet. "always" and "denyAlways" also need writing to the config file
// by the caller; the service applies them immediately either way.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT            L"approveRemoteClient"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT               L"denyRemoteClient"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS       L"getPendingRemoteClients"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE              L"scope"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_ONCE                   L"once"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_ALWAYS                 L"always"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_APPROVAL_SCOPE_UNTIL_RESTART          L"untilRestart"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_ADDRESS              L"remoteAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_REMOTE_PORT                 L"remotePort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_CLIENT_ENTRY_IDENTIFIER     L"entryIdentifier"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_PARAMETER_UMP_ENDPOINT_NAME           L"umpEndpointName"



#define MIDI_CONFIG_JSON_NETWORK_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS                L"enumerateClients"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CLIENTS_ARRAY_KEY       MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENTS_KEY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CONFIG_ID_KEY           L"entryIdentifier"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_HOST_ID_KEY             L"hostId"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_MDNS_MATCH_ID_KEY       L"mdnsMatchId"
//#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_ENABLED_KEY          MIDI_CONFIG_JSON_NETWORK_MIDI_ENABLED_KEY 
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_SESSION_ACTIVE_KEY   L"sessionActive"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_ADDRESS_KEY      L"remoteAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_REMOTE_PORT_KEY         L"remotePort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_ADDRESS_KEY       L"localAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_LOCAL_PORT_KEY          L"localPort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_UMP_ENDPOINT_ID_KEY     L"endpointDeviceId"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CREATE_MIDI1_PORTS_KEY  MIDI_CONFIG_JSON_NETWORK_MIDI_CREATE_MIDI1_PORTS_KEY

// A configured client is reported whether or not it is connected, so the app can show an entry
// which is not currently reachable.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_IS_DIRECT_KEY           L"isDirectConnection"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_DIRECT_ADDRESS_KEY      L"configuredDirectAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_DIRECT_PORT_KEY         L"configuredDirectPort"

// Where the entry is in its life. "unavailable" means a direct connection gave up and will only
// be tried again on a fresh connect command.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_ENTRY_STATE_KEY         L"entryState"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_PENDING                     L"pending"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_LIVE                        L"live"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_FAILED                      L"failed"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE                 L"unavailable"

#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_CURRENT_LATENCY_KEY                   L"currentLatencyTicks"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_RETRANSMIT_COUNT_KEY            L"totalRetransmitCount"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_RETRANSMIT_REQUEST_COUNT_KEY    L"totalRetransmitRequestCount"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_SENT_KEY        L"totalNetworkPacketsSent"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_CLIENTS_RESPONSE_TOTAL_NETWORK_PACKETS_RECEIVED_KEY    L"totalNetworkPacketsReceived"

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

// Per-host list of remote clients, so a polling app can show what is connected and what is
// waiting for the user to decide.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_CONNECTIONS_ARRAY_KEY     L"connections"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_ENUM_HOSTS_RESPONSE_REMOTE_CLIENT_POLICY_KEY  MIDI_CONFIG_JSON_NETWORK_MIDI_REMOTE_CLIENT_POLICY_KEY
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_PENDING_APPROVAL_KEY               L"pendingApproval"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_SESSION_ACTIVE_KEY                 L"sessionActive"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_ADDRESS_KEY                 L"remoteAddress"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_PORT_KEY                    L"remotePort"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_UMP_ENDPOINT_ID_KEY                L"endpointDeviceId"


// getPendingRemoteClients response. This is what the settings app polls, so each entry carries
// the three values approveRemoteClient / denyRemoteClient need, under the same key names those
// commands read, plus enough address detail to tell two similarly named devices apart.
//
// The identity keys are the existing MIDI_CONFIG_JSON_NETWORK_MIDI_CLIENT_IDENTITY_* ones, and
// the host entry identifier uses the command's own parameter key, so an entry can be handed
// back as command arguments without renaming anything.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENTS_RESPONSE_ARRAY_KEY            L"pendingRemoteClients"

// Which of this PC's hosts the remote is asking to join. Both are supplied because a user with
// several hosts running needs to know which one is being knocked on.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_HOST_NAME_KEY                  L"hostUmpEndpointName"
#define MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_HOST_SERVICE_INSTANCE_NAME_KEY L"hostServiceInstanceName"

// Address the invitation arrived from. This is the live socket address, not a stored one, and
// it changes between reconnects, so it is for display only and never for matching. The key is
// the shared MIDI_CONFIG_JSON_NETWORK_MIDI_CONNECTION_REMOTE_ADDRESS_KEY.

// ISO 8601 UTC, for example 2026-08-12T01:23:45.6789012Z. A string rather than a number because
// the JSON number type is a double and a FILETIME does not survive one intact.
#define MIDI_CONFIG_JSON_NETWORK_MIDI_PENDING_CLIENT_REQUEST_TIME_KEY               L"requestTime"



