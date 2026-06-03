// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32\
// UseLegacyMidi REG_DWORD
// we can't pull in the full MidiDefs file here because it includes a lot of 
// MIDL-incompatible stuff. So instead re-defining these

#define MIDI_USE_MIDISRV 0
#define MIDI_USE_LEGACY 1
#define MIDI_USE_HYBRID_LEGACY 2

