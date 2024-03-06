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

#define ABSTRACTION_LAYER_GUID __uuidof(Midi2DiagnosticsAbstraction);


#define TRANSPORT_MNEMONIC L"DIAG"
#define TRANSPORT_MANUFACTURER L"Microsoft"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID L"MIDIU_DIAG_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Diagnostics Devices"

#define DEFAULT_LOOPBACK_BIDI_A_ID L"MIDIU_DIAG_LOOPBACK_A"
#define LOOPBACK_BIDI_A_UNIQUE_ID L"LOOPBACK_A"
#define DEFAULT_LOOPBACK_BIDI_A_NAME L"Diagnostics Loopback A"

#define DEFAULT_LOOPBACK_BIDI_B_ID L"MIDIU_DIAG_LOOPBACK_B"
#define LOOPBACK_BIDI_B_UNIQUE_ID L"LOOPBACK_B"
#define DEFAULT_LOOPBACK_BIDI_B_NAME L"Diagnostics Loopback B"

#define DEFAULT_PING_BIDI_ID L"MIDIU_DIAG_PING"
#define PING_BIDI_UNIQUE_ID L"PING"
#define DEFAULT_PING_BIDI_NAME L"Diagnostics Ping (Internal)"





