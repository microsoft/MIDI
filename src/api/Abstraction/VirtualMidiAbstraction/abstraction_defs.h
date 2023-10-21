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

#define TRANSPORT_MNEMONIC L"VIRT"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID L"MIDIU_VIRTUAL_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Virtual Transport"


#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"

#define TRANSPORT_ENUMERATOR L"MIDISRV"
