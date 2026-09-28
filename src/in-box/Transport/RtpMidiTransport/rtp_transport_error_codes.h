// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Error codes returned in rtpMIDI transport configuration responses.
// ============================================================================

#pragma once

#define RTP_MIDI_ERROR_CODE_UNKNOWN_ERROR                   0
#define RTP_MIDI_ERROR_CODE_INVALID_JSON                    1
#define RTP_MIDI_ERROR_CODE_UNRECOGNIZED_COMMAND            2
#define RTP_MIDI_ERROR_CODE_MISSING_ENTRY_IDENTIFIER        3
#define RTP_MIDI_ERROR_CODE_INVALID_ENTRY_IDENTIFIER        4
#define RTP_MIDI_ERROR_CODE_ENTRY_NOT_FOUND                 5
#define RTP_MIDI_ERROR_CODE_INVALID_ENTRY                   6
#define RTP_MIDI_ERROR_CODE_NOT_READY                       7
#define RTP_MIDI_ERROR_CODE_MISSING_REMOTE                  8
#define RTP_MIDI_ERROR_CODE_NAME_TOO_LONG                   9
#define RTP_MIDI_ERROR_CODE_INVALID_PORT                    10
#define RTP_MIDI_ERROR_CODE_MISSING_CONNECTION_ID           11
#define RTP_MIDI_ERROR_CODE_INVALID_NAME                    12
#define RTP_MIDI_ERROR_CODE_MISSING_REMOTE_CLIENT_NAME      13
#define RTP_MIDI_ERROR_CODE_PENDING_REMOTE_CLIENT_NOT_FOUND 14
#define RTP_MIDI_ERROR_CODE_INVALID_APPROVAL_SCOPE          15
#define RTP_MIDI_ERROR_CODE_TOO_MANY_REMOTE_CLIENT_DECISIONS 16
#define RTP_MIDI_ERROR_CODE_CONNECTION_NOT_FOUND            17

// Raised in the SDK, never by the transport
#define RTP_MIDI_ERROR_CODE_CLIENT_API_SERVICE_UNAVAILABLE  1000
#define RTP_MIDI_ERROR_CODE_CLIENT_API_TIMEOUT              1001
#define RTP_MIDI_ERROR_CODE_CLIENT_API_INVALID_ARGUMENT     1002
#define RTP_MIDI_ERROR_CODE_CLIENT_API_EXCEPTION            1003
