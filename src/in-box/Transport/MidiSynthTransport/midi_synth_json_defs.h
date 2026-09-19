// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

// Shared between the synthesizer transport and the WinRT projection which talks to it. Both ends
// include this file so a renamed key cannot compile on one side and silently stop matching on the
// other.

#pragma once

#ifndef MIDI_SYNTH_JSON_DEFS_H
#define MIDI_SYNTH_JSON_DEFS_H

// Settings, as they appear in the configuration file and in a status reply.

#define MIDI_SYNTH_JSON_ENABLED_PROPERTY_KEY            L"enabled"
#define MIDI_SYNTH_JSON_SYNTH_MODE_PROPERTY_KEY         L"synthMode"
#define MIDI_SYNTH_JSON_AUDIO_MODE_PROPERTY_KEY         L"audioMode"
#define MIDI_SYNTH_JSON_BANK_SELECT_PROPERTY_KEY        L"bankSelectMode"
#define MIDI_SYNTH_JSON_VOLUME_PROPERTY_KEY             L"volumeDecibels"
#define MIDI_SYNTH_JSON_EFFECTS_PROPERTY_KEY            L"effectsEnabled"

// Reported, never accepted: the transport owns its endpoint's identity.
#define MIDI_SYNTH_JSON_ENDPOINT_DEVICE_ID_KEY          L"endpointDeviceId"

#define MIDI_SYNTH_JSON_SYNTH_MODE_COMPATIBLE           L"compatible"
#define MIDI_SYNTH_JSON_SYNTH_MODE_MODERN               L"modern"

#define MIDI_SYNTH_JSON_AUDIO_MODE_SHARED               L"shared"
#define MIDI_SYNTH_JSON_AUDIO_MODE_SHARED_LOW_LATENCY   L"sharedLowLatency"
#define MIDI_SYNTH_JSON_AUDIO_MODE_EXCLUSIVE            L"exclusive"
#define MIDI_SYNTH_JSON_AUDIO_MODE_ASIO                 L"asio"

#define MIDI_SYNTH_JSON_BANK_SELECT_GS                  L"gs"
#define MIDI_SYNTH_JSON_BANK_SELECT_XG                  L"xg"
#define MIDI_SYNTH_JSON_BANK_SELECT_GM2                 L"gm2"
#define MIDI_SYNTH_JSON_BANK_SELECT_AUTOMATIC           L"automatic"

// Command verbs.

#define MIDI_SYNTH_COMMAND_STATUS                       L"status"
#define MIDI_SYNTH_COMMAND_ENABLE                       L"enable"
#define MIDI_SYNTH_COMMAND_DISABLE                      L"disable"
#define MIDI_SYNTH_COMMAND_SOUND_SET                    L"soundset"
#define MIDI_SYNTH_COMMAND_INSTRUMENT_LIST              L"instrumentlist"
#define MIDI_SYNTH_COMMAND_SET_DRUM_CHANNEL             L"setdrumchannel"

#define MIDI_SYNTH_COMMAND_ARG_CHANNEL                  L"channel"
#define MIDI_SYNTH_COMMAND_ARG_IS_DRUM_CHANNEL          L"isDrumChannel"

// Sound set reply.

#define MIDI_SYNTH_JSON_SOUND_SET_NAME_KEY              L"soundSetName"
#define MIDI_SYNTH_JSON_SOUND_SET_VERSION_KEY           L"soundSetVersion"
#define MIDI_SYNTH_JSON_SOUND_SET_PATH_KEY              L"soundSetPath"
#define MIDI_SYNTH_JSON_SOUND_SET_INSTRUMENTS_KEY       L"instrumentCount"
#define MIDI_SYNTH_JSON_SOUND_SET_WAVES_KEY             L"waveCount"
#define MIDI_SYNTH_JSON_SOUND_SET_MELODIC_KEY           L"melodicCount"
#define MIDI_SYNTH_JSON_SOUND_SET_KITS_KEY              L"drumKits"
#define MIDI_SYNTH_JSON_SOUND_SET_KIT_NAME_KEY          L"name"

// Melodic instrument list. Separate from the sound set reply because it is hundreds of entries and
// most callers only want the counts. Fixed for a given sound set: configuration changes how an
// incoming bank select is read, not what the sound set contains.
#define MIDI_SYNTH_JSON_INSTRUMENTS_KEY                 L"instruments"
#define MIDI_SYNTH_JSON_INSTRUMENT_NAME_KEY             L"name"
#define MIDI_SYNTH_JSON_INSTRUMENT_BANK_MSB_KEY         L"bankMsb"
#define MIDI_SYNTH_JSON_INSTRUMENT_BANK_LSB_KEY         L"bankLsb"
#define MIDI_SYNTH_JSON_INSTRUMENT_PROGRAM_KEY          L"program"
#define MIDI_SYNTH_JSON_SOUND_SET_KIT_PROGRAM_KEY       L"program"

#endif
