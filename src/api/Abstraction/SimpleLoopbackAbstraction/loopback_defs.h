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

// Names are not final. Need to see if we'll get a different structure in device manager, and also
// how these will be presented through apps. The two tend to be at odds with each other, but we'll
// err on the side of app presentation.

#define TRANSPORT_MNEMONIC L"LOOP"

#define TRANSPORT_PARENT_ID L"MIDIU_LOOPBACK_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Loopback Transport"

#define DEFAULT_LOOPBACK_BIDI_A_ID L"MIDIU_DEFAULT_LOOPBACK_BIDI_A"
#define DEFAULT_LOOPBACK_BIDI_A_NAME L"Loopback UMP Endpoint A (BiDi)"

#define DEFAULT_LOOPBACK_BIDI_B_ID L"MIDIU_DEFAULT_LOOPBACK_BIDI_B"
#define DEFAULT_LOOPBACK_BIDI_B_NAME L"Loopback UMP Endpoint B (BiDi)"


#define DEFAULT_LOOPBACK_OUT_ID L"MIDIU_DEFAULT_LOOPBACK_OUT"
#define DEFAULT_LOOPBACK_OUT_NAME L"Loopback UMP Endpoint (Out)"

#define DEFAULT_LOOPBACK_IN_ID L"MIDIU_DEFAULT_LOOPBACK_IN"
#define DEFAULT_LOOPBACK_IN_NAME L"Loopback UMP Endpoint (In)"


#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"

#define TRANSPORT_ENUMERATOR L"MIDISRV"


