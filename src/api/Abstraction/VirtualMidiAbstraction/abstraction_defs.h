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

#define ABSTRACTION_LAYER_GUID __uuidof(Midi2VirtualMidiAbstraction);

#define TRANSPORT_MANUFACTURER L"Microsoft"
#define TRANSPORT_MNEMONIC L"APP"
#define MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX L"MIDIU_APPDEV_"
#define MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX L"MIDIU_APPPUB_"

// TODO: Names should be moved to .rc for localization

#define TRANSPORT_PARENT_ID L"MIDIU_APP_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 App Devices"


#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"

#define TRANSPORT_ENUMERATOR L"MIDISRV"

