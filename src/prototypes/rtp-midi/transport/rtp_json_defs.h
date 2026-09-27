// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// PROTOTYPE. JSON keys for the rtpMIDI transport's configuration section and commands.
//
// Names follow the Network MIDI 2.0 transport wherever the meaning is the same, so one settings
// app can read both. The word "session" is avoided: it already means something else in Windows
// MIDI Services.
// ============================================================================

#pragma once

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


// Commands
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_HOSTS          L"enumerateHosts"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_CLIENTS        L"enumerateClients"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_ENUMERATE_ADVERTISED     L"enumerateAdvertisedPeers"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_START_HOST               L"startHost"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_STOP_HOST                L"stopHost"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_HOST              L"removeHost"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_REMOVE_CLIENT            L"removeClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_RECONNECT_CLIENT         L"reconnectClient"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_VERB_DISCONNECT_REMOTE        L"disconnectRemoteClient"

#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_ENTRY_IDENTIFIER    L"entryIdentifier"
#define MIDI_CONFIG_JSON_RTP_MIDI_COMMAND_PARAMETER_CONNECTION_ID       L"connectionId"


// Responses
#define MIDI_CONFIG_JSON_RTP_MIDI_ENTRY_IDENTIFIER_KEY                  L"entryIdentifier"
#define MIDI_CONFIG_JSON_RTP_MIDI_ADVERTISED_PEERS_KEY                  L"advertisedPeers"

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
