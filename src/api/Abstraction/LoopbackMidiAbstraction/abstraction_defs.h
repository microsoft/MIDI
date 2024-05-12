// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// the IDs here aren't the full Ids, just the values we start with
// The full Id comes back from the swdevicecreate callback

#define ABSTRACTION_LAYER_GUID __uuidof(Midi2LoopbackMidiAbstraction);

#define TRANSPORT_MANUFACTURER L"Microsoft"
#define TRANSPORT_MNEMONIC L"LOOP"

#define MIDI_PERM_LOOP_INSTANCE_ID_A_PREFIX L"MIDIU_LOOP_A_"
#define MIDI_PERM_LOOP_INSTANCE_ID_B_PREFIX L"MIDIU_LOOP_B_"

#define MIDI_TEMP_LOOP_INSTANCE_ID_A_PREFIX L"MIDIU_LOOP_A_RT_"
#define MIDI_TEMP_LOOP_INSTANCE_ID_B_PREFIX L"MIDIU_LOOP_B_RT_"


// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID L"MIDIU_LOOP_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Loop Devices"
#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"


#define TRANSPORT_ENUMERATOR L"MIDISRV"

