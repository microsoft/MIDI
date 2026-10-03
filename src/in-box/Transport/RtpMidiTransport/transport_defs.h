// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// rtpMIDI (RTP-MIDI, RFC 6295, with Apple's session protocol) transport.
// ============================================================================

#pragma once

#define TRANSPORT_LAYER_GUID                                    __uuidof(Midi2RtpMidiTransport)

#define TRANSPORT_MANUFACTURER                                  L"Microsoft"
#define TRANSPORT_CODE                                          L"RTPMIDI"

#define TRANSPORT_PARENT_ID                                     L"MIDIU_RTPMIDI_TRANSPORT"

#define MIDI_RTP_ENDPOINT_INSTANCE_ID_PREFIX                    L"MIDIU_RTPMIDI_"
#define MIDI_RTP_ENDPOINT_INSTANCE_ID_NAME_MAX_CHARS            24

#define MIDI_RTP_DNSSD_SERVICE_TYPE                             L"_apple-midi._udp.local"

// macOS, iOS and rtpMIDI for Windows all default to 5004, with data on 5005.
#define MIDI_RTP_DEFAULT_HOST_PORT                              5004

// Where a host looks when its port is taken, for example by rtpMIDI for Windows running a session
// on 5004. The first range keeps the port where other rtpMIDI software expects to find one.
#define MIDI_RTP_HOST_FALLBACK_FIRST_PORT                       5006
#define MIDI_RTP_HOST_FALLBACK_LAST_PORT                        5100
#define MIDI_RTP_SECONDARY_FALLBACK_FIRST_PORT                  45000
#define MIDI_RTP_SECONDARY_FALLBACK_LAST_PORT                   45998

// A client only needs a pair nobody else holds. Below the Windows dynamic range, so the OS does
// not hand the ports to another process between reconnects.
#define MIDI_RTP_CLIENT_FIRST_PORT                              46000
#define MIDI_RTP_CLIENT_LAST_PORT                               48998

// DNS-SD instance labels are limited to 63 bytes of UTF-8. The AppleMIDI name carries the same
// limit here so the name a peer is shown always fits in its directory.
#define MIDI_RTP_NAME_MAX_UTF8_BYTES                            63

#define MIDI_RTP_MAX_HOST_CONNECTIONS                           16

// A remote waiting for a decision stops asking after about twelve seconds. Its request stays
// listed this long after its last ask, and a one-time approval waits this long for it to return.
#define MIDI_RTP_PENDING_REMOTE_CLIENT_LIFETIME_MS              120000

// Anyone on the network can send an invitation, so what they can make this PC remember is bounded
#define MIDI_RTP_MAX_PENDING_REMOTE_CLIENTS_PER_HOST            16
#define MIDI_RTP_MAX_REMOTE_CLIENT_DECISIONS_PER_HOST           256
#define MIDI_RTP_REMOTE_CLIENT_NAME_MAX_CHARS                   255

// The configuration file is written by customers and tools, so text that only this PC uses, like a
// custom endpoint name or a remote's host name, is bounded as well. DNS host names stop at 253.
#define MIDI_RTP_CONFIG_TEXT_MAX_CHARS                          255

// How long the worker waits for a remote's host name to resolve before trying again later
#define MIDI_RTP_NAME_RESOLUTION_TIMEOUT_SECONDS                5

// How often the worker looks at hosts and clients that are not running as configured.
#define MIDI_RTP_WORKER_INTERVAL_MS                             1000

// A client whose remote did not answer waits this long before inviting it again. Longer than the
// twelve second invitation cycle, so two cycles never overlap.
#define MIDI_RTP_CLIENT_RETRY_INTERVAL_MS                       15000

// A host limited to a network adapter follows it as it comes and goes. Changes are noticed when
// they happen, and once in this long anyway in case one came and went unseen.
#define MIDI_RTP_NETWORK_ADAPTER_CHECK_INTERVAL_MS              20000

// An adapter coming up changes several addresses in a row. The worker looks once they settle.
#define MIDI_RTP_NETWORK_ADAPTER_SETTLE_MS                      2000

// Longer than any hardware address Windows reports
#define MIDI_RTP_NETWORK_ADAPTER_PHYSICAL_ADDRESS_MAX_CHARS     64

// How often the measured latency is written to the endpoint for the outbound scheduler, and the
// smallest change worth a device property write.
#define MIDI_RTP_LATENCY_PROPERTY_INTERVAL_MS                   5000
#define MIDI_RTP_LATENCY_PROPERTY_THRESHOLD_MICROSECONDS        1000

// Engine timer resolution. AppleMIDI timers are in hundreds of milliseconds, so this only has to
// be fine enough for invitation retries and clock sync to be on time.
#define MIDI_RTP_TICK_INTERVAL_MS                               10

// With a send speed limit, a connection's queue holds about this much time at that speed before
// senders wait for room, so a message sent behind a burst is not held up behind a long queue
#define MIDI_RTP_SEND_QUEUE_PACED_MILLISECONDS                  100
#define MIDI_RTP_SEND_QUEUE_PACED_MINIMUM_BYTES                 256

// Without one, nothing stays in the queue for long, so this is only for safety
#define MIDI_RTP_SEND_QUEUE_UNLIMITED_MAX_BYTES                 (256 * 1024)

// A sender waits for room at most this long, then its messages are queued anyway. Kept under the
// 1 second an app's side of the service pipe waits, so the app never sees a stall.
#define MIDI_RTP_SEND_QUEUE_WAIT_LIMIT_MILLISECONDS             900
#define MIDI_RTP_SEND_QUEUE_WAIT_SLICE_MILLISECONDS             50

// Only reached when nothing is draining the queue. Messages past this are dropped, with a trace.
#define MIDI_RTP_SEND_QUEUE_HARD_MAX_BYTES                      (1024 * 1024)
