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

#define TRANSPORT_MNEMONIC L"DIAG"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID L"MIDIU_LOOPBACK_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Diagnostics Transport"

#define DEFAULT_LOOPBACK_BIDI_A_ID L"MIDIU_LOOPBACK_A"
#define DEFAULT_LOOPBACK_BIDI_A_NAME L"Loopback Endpoint A (MIDI 2.0)"

#define DEFAULT_LOOPBACK_BIDI_B_ID L"MIDIU_LOOPBACK_B"
#define DEFAULT_LOOPBACK_BIDI_B_NAME L"Loopback Endpoint B (MIDI 2.0)"

#define DEFAULT_PING_BIDI_ID L"MIDIU_PING"
#define DEFAULT_PING_BIDI_NAME L"Ping UMP Endpoint (Internal)"



#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"

#define TRANSPORT_ENUMERATOR L"MIDISRV"


