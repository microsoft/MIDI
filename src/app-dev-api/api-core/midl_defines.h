// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

// These are required for internal Microsoft build support and if set incorrectly
// will break any public compilation.

#ifdef RAZZLE

#define MIDI_API_CONTRACT_1 contract(Windows.Foundation.UniversalApiContract, 1) 
#define MIDI_IDL_IMPORT import "Windows.Foundation.idl";

#else

#define MIDI_API_CONTRACT_1
#define MIDI_IDL_IMPORT

#endif