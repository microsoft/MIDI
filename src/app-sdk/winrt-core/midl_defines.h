// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// These are required for internal Microsoft build support and if set incorrectly
// will break any public compilation.

#ifdef MICROSOFT_INTERNAL_BUILD

// internal build

#define MIDI_API_CONTRACT_v1     [contract(Microsoft.Devices.Midi2ApiContract, 1)]

#define MIDI_IDL_IMPORT \
import "Midi2ApiContract.idl"; \
import "Windows.Foundation.idl"; \
import "Windows.Devices.Enumeration.idl"; \
import "Windows.Data.Json.idl"; \
import "Windows.Devices.Midi.idl"; \
import "Windows.Devices.Enumeration.idl"; \

#define MIDI_INTERFACE_UUID(u,v) uuid(u)

#else

// public build (using Visual Studio, msbuild, etc.)

#define MIDI_API_CONTRACT_v1
#define MIDI_IDL_IMPORT
#define MIDI_INTERFACE_UUID(u,v) uuid(u), version(v)

#endif


// common MIDL defines

#define MIDI_TIMESTAMP UInt64
