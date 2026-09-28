// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// JSON keys for the rtpMIDI transport's configuration section and commands.
//
// Names follow the Network MIDI 2.0 transport wherever the meaning is the same, so one settings
// app can read both. The word "session" is avoided: it already means something else in Windows
// MIDI Services.
// ============================================================================

#pragma once

// The transport's COM class id, which is also its key in the configuration file
#define MIDI_RTP_TRANSPORT_ID                                           L"{54C9B2F6-C235-4000-A675-9F6958A1A4FA}"

// Configuration file section, under create / remove, keyed by entry identifier (a GUID)
#define MIDI_CONFIG_JSON_RTP_MIDI_HOSTS_KEY                             L"hosts"
#define MIDI_CONFIG_JSON_RTP_MIDI_CLIENTS_KEY                           L"clients"

// The name peers see in invitations and in their participant lists
#define MIDI_CONFIG_JSON_RTP_MIDI_NAME_KEY                              L"name"

// Host: the DNS-SD label to advertise. Client: the advertised remote to connect to.
#define MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_KEY             L"serviceInstanceName"

#define MIDI_CONFIG_JSON_RTP_MIDI_PORT_KEY                              L"port"
#define MIDI_CONFIG_JSON_RTP_MIDI_PORT_VALUE_AUTO                       L"auto"
#define MIDI_CONFIG_JSON_RTP_MIDI_ALLOW_PORT_FALLBACK_KEY               L"allowPortFallback"
#define MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISE_KEY                         L"advertise"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENABLED_KEY                           L"enabled"

// Client configured by address rather than by discovery
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_ADDRESS_KEY                    L"remoteAddress"
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_PORT_KEY                       L"remotePort"

// Same meaning as in Network MIDI 2.0: the endpoint is created under this name, never renamed later
#define MIDI_CONFIG_JSON_RTP_MIDI_CUSTOM_ENDPOINT_NAME_KEY              L"customEndpointName"

#define MIDI_CONFIG_JSON_RTP_MIDI_AUTO_RECONNECT_KEY                    L"autoReconnect"

// Chapter N recovery journal on outgoing packets, so a peer can repair a lost Note Off
#define MIDI_CONFIG_JSON_RTP_MIDI_SEND_RECOVERY_JOURNAL_KEY             L"sendRecoveryJournal"

// Who may connect to a host. Same key and values as Network MIDI 2.0. Missing means allowAny,
// and any other value means requireApproval, so a damaged entry never opens a host up.
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_KEY              L"remoteClientPolicy"
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_ALLOW_ANY  L"allowAny"
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_POLICY_VALUE_REQUIRE_APPROVAL L"requireApproval"

// Decisions to remember, under create and keyed by host entry identifier. Kept apart from the
// host so an app can save them without rewriting the host's definition. Each list entry is an
// object holding remoteName: RTP-MIDI carries nothing else that stays the same between connections.
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_CLIENT_DECISIONS_KEY           L"remoteClientDecisions"
#define MIDI_CONFIG_JSON_RTP_MIDI_ALLOWED_CLIENTS_KEY                   L"allowedClients"
#define MIDI_CONFIG_JSON_RTP_MIDI_DENIED_CLIENTS_KEY                    L"deniedClients"


// Commands
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS          L"enumerateHosts"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS        L"enumerateClients"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED     L"enumerateAdvertisedHosts"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST               L"startHost"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_STOP_HOST                L"stopHost"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST              L"removeHost"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_CLIENT            L"removeClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_RECONNECT_CLIENT         L"reconnectClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DISCONNECT_REMOTE        L"disconnectRemoteClient"

// Decisions about a remote that wants to connect to a host, named by the name it sends. The
// service applies them at once. "always" also needs saving to the configuration file by the
// caller, and forgetting one needs it taken out of the file.
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_APPROVE_REMOTE_CLIENT    L"approveRemoteClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DENY_REMOTE_CLIENT       L"denyRemoteClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_FORGET_REMOTE_CLIENT     L"forgetRemoteClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_GET_PENDING_REMOTE_CLIENTS L"getPendingRemoteClients"

#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_REMOTE_NAME         L"remoteName"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_APPROVAL_SCOPE      L"scope"
#define MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_ONCE                   L"once"
#define MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_UNTIL_RESTART          L"untilRestart"
#define MIDI_CONFIG_JSON_RTP_MIDI_APPROVAL_SCOPE_ALWAYS                 L"always"

#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_ENTRY_IDENTIFIER    L"entryIdentifier"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_CONNECTION_ID       L"connectionId"


// Responses
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY                  L"entryIdentifier"
#define MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISED_HOSTS_KEY                  L"advertisedHosts"

#define MIDI_CONFIG_JSON_RTP_MIDI_HAS_STARTED_KEY                       L"hasStarted"
#define MIDI_CONFIG_JSON_RTP_MIDI_CONFIGURED_PORT_KEY                   L"configuredPort"
#define MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_PORT_KEY                       L"actualPort"
#define MIDI_CONFIG_JSON_RTP_MIDI_PORT_FALLBACK_USED_KEY                L"portFallbackUsed"
#define MIDI_CONFIG_JSON_RTP_MIDI_ACTUAL_SERVICE_INSTANCE_NAME_KEY      L"actualServiceInstanceName"
#define MIDI_CONFIG_JSON_RTP_MIDI_SERVICE_INSTANCE_NAME_CHANGED_KEY     L"serviceInstanceNameChanged"
#define MIDI_CONFIG_JSON_RTP_MIDI_LAST_ERROR_KEY                        L"lastError"
#define MIDI_CONFIG_JSON_RTP_MIDI_CONNECTIONS_KEY                       L"connections"

#define MIDI_CONFIG_JSON_RTP_MIDI_IS_DIRECT_KEY                         L"isDirectConnection"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_KEY                       L"entryState"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_PENDING             L"pending"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_LIVE                L"live"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_FAILED              L"failed"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_STATE_VALUE_UNAVAILABLE         L"unavailable"

// One connection, whichever side of it this PC is on
#define MIDI_CONFIG_JSON_RTP_MIDI_CONNECTION_ID_KEY                     L"connectionId"
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_NAME_KEY                       L"remoteName"
#define MIDI_CONFIG_JSON_RTP_MIDI_LOCAL_PORT_KEY                        L"localPort"

// The host name from the remote's rtpMIDI advertisement, for matching it with the same device on
// Network MIDI 2.0. Empty when no single advertised host lists the remote's address.
#define MIDI_CONFIG_JSON_RTP_MIDI_REMOTE_HOST_NAME_KEY                  L"remoteHostName"
#define MIDI_CONFIG_JSON_RTP_MIDI_CONNECTION_STATE_KEY                  L"connectionState"
#define MIDI_CONFIG_JSON_RTP_MIDI_IS_CONNECTED_KEY                      L"connected"
#define MIDI_CONFIG_JSON_RTP_MIDI_WE_INITIATED_KEY                      L"thisPcInvited"
#define MIDI_CONFIG_JSON_RTP_MIDI_ENDPOINT_DEVICE_ID_KEY                L"endpointDeviceId"

// Same key names and units as Network MIDI 2.0. Latency is the clock sync round trip, in MIDI
// timestamp ticks, averaged over the samples the clock filter holds.
#define MIDI_CONFIG_JSON_RTP_MIDI_CURRENT_LATENCY_KEY                   L"currentLatencyTicks"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_SENT_KEY                L"totalNetworkPacketsSent"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_RECEIVED_KEY            L"totalNetworkPacketsReceived"

// rtpMIDI has no retransmission. These are what takes its place in a connection detail panel.
#define MIDI_CONFIG_JSON_RTP_MIDI_BEST_LATENCY_KEY                      L"bestLatencyTicks"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_PACKETS_LOST_KEY                L"totalPacketsLost"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_LOSSES_REPAIRED_KEY             L"totalLossesRepairedFromJournal"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_NOTES_ENDED_KEY                 L"totalNoteOffsRecovered"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_SENT_KEY               L"totalMessagesSent"
#define MIDI_CONFIG_JSON_RTP_MIDI_TOTAL_MESSAGES_RECEIVED_KEY           L"totalMessagesReceived"

// Advertised peer
#define MIDI_CONFIG_JSON_RTP_MIDI_HOST_NAME_KEY                         L"hostName"
#define MIDI_CONFIG_JSON_RTP_MIDI_IPV4_ADDRESSES_KEY                    L"ipv4Addresses"
#define MIDI_CONFIG_JSON_RTP_MIDI_IPV6_ADDRESSES_KEY                    L"ipv6Addresses"

// A host this PC advertises shows up in its own browse results, and a picker should leave it out
#define MIDI_CONFIG_JSON_RTP_MIDI_IS_THIS_PC_KEY                        L"isThisPc"

// getPendingRemoteClients. Each entry carries the host's entryIdentifier and the remoteName, the
// arguments a decision needs, under the same keys the commands read.
#define MIDI_CONFIG_JSON_RTP_MIDI_PENDING_REMOTE_CLIENTS_KEY            L"pendingRemoteClients"
#define MIDI_CONFIG_JSON_RTP_MIDI_PENDING_HOST_NAME_KEY                 L"hostName"
#define MIDI_CONFIG_JSON_RTP_MIDI_PENDING_HOST_SERVICE_INSTANCE_NAME_KEY L"hostServiceInstanceName"

// ISO 8601 UTC. A string, because a FILETIME does not survive a JSON number intact.
#define MIDI_CONFIG_JSON_RTP_MIDI_PENDING_REQUEST_TIME_KEY              L"requestTime"

// Approved for one connection, and waiting for the remote to ask again
#define MIDI_CONFIG_JSON_RTP_MIDI_PENDING_APPROVED_KEY                  L"approved"

// In enumerateHosts, on each allowedClients and deniedClients entry: forgotten when the service restarts
#define MIDI_CONFIG_JSON_RTP_MIDI_UNTIL_RESTART_KEY                     L"untilRestart"
