// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#define TRANSPORT_LAYER_GUID __uuidof(Midi2MidiSynthTransport);

#define TRANSPORT_MANUFACTURER L"Microsoft"
#define TRANSPORT_CODE L"GMSYNTH"

// client SDK uses copies of these values. Do not change.

#define MIDI_SYNTH_INSTANCE_ID_PREFIX L"MIDIU_GMSYNTH_"

// There is exactly one synthesizer, so its identifier is fixed rather than generated. A stable
// endpoint device id is the point: applications and WinMM port assignments remember it.
#define MIDI_SYNTH_ENDPOINT_UNIQUE_ID L"GM1"

#define TRANSPORT_PARENT_ID L"MIDIU_GMSYNTH_TRANSPORT"

#define TRANSPORT_ENUMERATOR L"MIDISRV"


// General MIDI is defined over sixteen channels, which is exactly one UMP Group, and the UMP
// specification wants an input and output intended to work as a pair to be a single bidirectional
// Function Block over a single Group.
#define MIDI_SYNTH_GROUP_INDEX 0


// JSON keys and command verbs are shared with the WinRT projection.
#include "midi_synth_json_defs.h"

