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

#define TRANSPORT_PARENT_ID L"MIDIU_VIRT_TRANSPORT"
#define TRANSPORT_PARENT_DEVICE_NAME L"MIDI 2.0 Virtual Device Enumerator"


#define LOOPBACK_PARENT_ROOT L"HTREE\\ROOT\\0"

#define TRANSPORT_ENUMERATOR L"MIDISRV"


//"createVirtualDevice":
//{
//    "associationIdentifier": "{5A5A47C9-6750-4068-BBC9-DD43AB0B8EF4}",
//    "name" : "Pete's App",
//    "description" : "Virtual app-to-app MIDI example device"
//}
#define MIDI_VIRT_JSON_INSTRUCTION_CREATE_KEY                   L"createVirtualDevice"
#define MIDI_VIRT_JSON_INSTRUCTION_UPDATE_KEY                   L"updateVirtualDevice"

// this will only make sense for runtime configuration changes once that is in place
#define MIDI_VIRT_JSON_INSTRUCTION_REMOVE_KEY                   L"removeVirtualDevice"

// "associationIdentifier": "{5A5A47C9-6750-4068-BBC9-DD43AB0B8EF4}"
#define MIDI_VIRT_JSON_DEVICE_PROPERTY_ASSOCIATION_IDENTIFIER   L"associationIdentifier"

// "name": "Pete's App"
#define MIDI_VIRT_JSON_DEVICE_PROPERTY_NAME                     L"name"

//"description" : "Virtual app-to-app MIDI example device"
#define MIDI_VIRT_JSON_DEVICE_PROPERTY_DESCRIPTION              L"description"
