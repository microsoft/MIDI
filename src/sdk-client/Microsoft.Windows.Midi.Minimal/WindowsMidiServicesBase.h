// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

#pragma once

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include <cstdint>
#include <Windows.h>

namespace Microsoft::Windows::Midi
{
	using MidiObjectId = GUID;

	// these are generally there to signify intent

	using MidiWord32 = uint32_t;
	using MidiShort16 = uint16_t;
	using MidiShort14 = uint16_t;
	using MidiByte8 = uint8_t;
	using MidiByte7 = uint8_t;
	using MidiNibble4 = uint8_t;



	enum WINDOWSMIDISERVICES_API MidiMessageType : MidiNibble4
	{
		MidiMessageTypeUtility = 0x0,
		MidiMessageTypeSystem = 0x1,
		MidiMessageTypeMidi1ChannelVoice = 0x2,
		MidiMessageTypeSystemExclusive7Bit = 0x3,
		MidiMessageTypeMidi2ChannelVoice = 0x4,
		MidiMessageTypeSystemExclusive8Bit = 0x5,

	};

	enum WINDOWSMIDISERVICES_API Midi1ChannelVoiceOpcode : MidiNibble4
	{
		Midi1ChannelVoiceOpcodeNoteOff = 0x8,
		Midi1ChannelVoiceOpcodeNoteOn = 0x9,
		Midi1ChannelVoiceOpcodePolyPressure = 0xA,
		Midi1ChannelVoiceOpcodeControlChange = 0xB,
		Midi1ChannelVoiceOpcodeProgramChange = 0xC,
		Midi1ChannelVoiceOpcodeChannelPressure = 0xD,
		Midi1ChannelVoiceOpcodePitchBend = 0xE
	};

	enum WINDOWSMIDISERVICES_API Midi2ChannelVoiceOpcode : MidiNibble4
	{
		Midi2ChannelVoiceOpcodeNoteOff = 0x8,
		Midi2ChannelVoiceOpcodeNoteOn = 0x9,
		Midi2ChannelVoiceOpcodePolyPressure = 0xA,
		Midi2ChannelVoiceOpcodeRegisteredPerNoteController = 0x0,
		Midi2ChannelVoiceOpcodeAssignablePerNoteController = 0x1,
		Midi2ChannelVoiceOpcodePerNoteManagement = 0xF,
		Midi2ChannelVoiceOpcodeControlChange = 0xB,
		Midi2ChannelVoiceOpcodeRegisteredController = 0x2,
		Midi2ChannelVoiceOpcodeAssignableController = 0x3,
		Midi2ChannelVoiceOpcodeRelativeRegisteredController = 0x4,
		Midi2ChannelVoiceOpcodeRelativeAssignableController = 0x5,
		Midi2ChannelVoiceOpcodeProgramChange = 0xC,
		Midi2ChannelVoiceOpcodeChannelPressure = 0xD,
		Midi2ChannelVoiceOpcodePitchBend = 0xE,
		Midi2ChannelVoiceOpcodePerNotePitchBend = 0x6
	};

	// Status nibble for a 7 bit sysex message
	enum WINDOWSMIDISERVICES_API SystemExclusive7MessageStatus : MidiNibble4
	{
		SysEx7CompleteInOneUmp = 0x0,
		SysEx7StartUmp = 0x1,
		SysEx7ContinueUmp = 0x2,
		SysEx7EndUmp = 0x3
	};

	// Status nibble for an 8 bit sysex message
	enum WINDOWSMIDISERVICES_API SystemExclusive8MessageStatus : MidiNibble4
	{
		SysEx8CompleteInOneUmp = 0x0,
		SysEx8StartUmp = 0x1,
		SysEx8ContinueUmp = 0x2,
		SysEx8EndUmp = 0x3
	};


}

