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

namespace Microsoft::Windows::Midi //::inline v0_1_0_pre
{
	using MidiObjectId = GUID;

	// these are generally there to signify intent
	// Didn't create bitfield structs or anything because
	// that's overkill, and makes using them a pain.

	using MidiWord32 = uint32_t;		// 32 bit MIDI word
	using MidiShort16 = uint16_t;		// 16 bit half-word
	using MidiShort14 = uint16_t;		// 14 bits of data in a 16 bit half-word
	using MidiByte8 = uint8_t;			// 8 bits of data
	using MidiByte7 = uint8_t;			// 7 bits of data, common with MIDI 1.0 and anything there for compatibility with 1.0
	using MidiNibble4 = uint8_t;		// 4 bits. Half a byte. Very common in MIDI messages for message type/group/channel/opcode
	using MidiNibble3 = uint8_t;		// 3 bits. MIDI Time Code Quarter Frame and some others


	// this is here to enforce 0-15 numbering for folks who
	// may be unfamiliar with it, and to avoid ambiguity
	// those who are familiar can just cast the value to this type
	enum WINDOWSMIDISERVICES_API MidiChannel : MidiNibble4
	{
		MidiChannel01 = 0,
		MidiChannel02 = 1,
		MidiChannel03 = 2,
		MidiChannel04 = 3,
		MidiChannel05 = 4,
		MidiChannel06 = 5,
		MidiChannel07 = 6,
		MidiChannel08 = 7,
		MidiChannel09 = 8,
		MidiChannel10 = 9,
		MidiChannel11 = 10,
		MidiChannel12 = 11,
		MidiChannel13 = 12,
		MidiChannel14 = 13,
		MidiChannel15 = 14,
		MidiChannel16 = 15
	};


	enum WINDOWSMIDISERVICES_API MidiGroup : MidiNibble4
	{
		MidiGroup01 = 0,
		MidiGroup02 = 1,
		MidiGroup03 = 2,
		MidiGroup04 = 3,
		MidiGroup05 = 4,
		MidiGroup06 = 5,
		MidiGroup07 = 6,
		MidiGroup08 = 7,
		MidiGroup09 = 8,
		MidiGroup10 = 9,
		MidiGroup11 = 10,
		MidiGroup12 = 11,
		MidiGroup13 = 12,
		MidiGroup14 = 13,
		MidiGroup15 = 14,
		MidiGroup16 = 15
	};



	enum WINDOWSMIDISERVICES_API MidiMessageType : MidiNibble4
	{
		MidiMessageTypeUtility = 0x0,
		MidiMessageTypeSystem = 0x1,
		MidiMessageTypeMidi1ChannelVoice = 0x2,
		MidiMessageTypeSystemExclusive7Bit = 0x3,
		MidiMessageTypeMidi2ChannelVoice = 0x4,
		MidiMessageTypeMidi2Data = 0x5
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


	enum WINDOWSMIDISERVICES_API Midi2NoteOnOffAttributeType : MidiByte8
	{
		Midi2NoteOnOffAttributeTypeNoData = 0x00,
		Midi2NoteOnOffAttributeTypeManufacturerSpecific = 0x01,
		Midi2NoteOnOffAttributeTypeProfileSpecific = 0x02,
		Midi2NoteOnOffAttributeTypePitchSevenDotNine = 0x03
	};


	enum WINDOWSMIDISERVICES_API Midi2ProgramChangeOptionFlags : MidiByte8
	{
		Midi2ProgramChangeNoOptions = 0,
		Midi2ProgramChangeBankValid = 1
	};



	// Status nibble for a 7 bit sysex message
	enum WINDOWSMIDISERVICES_API MidiSystemExclusive7MessageStatus : MidiNibble4
	{
		MidiSysEx7CompleteInOneUmp = 0x0,
		MidiSysEx7StartUmp = 0x1,
		MidiSysEx7ContinueUmp = 0x2,
		MidiSysEx7EndUmp = 0x3
	};

	// Status nibble for an 8 bit sysex message
	enum WINDOWSMIDISERVICES_API MidiSystemExclusive8MessageStatus : MidiNibble4
	{
		MidiSysEx8CompleteInOneUmp = 0x0,
		MidiSysEx8StartUmp = 0x1,
		MidiSysEx8ContinueUmp = 0x2,
		MidiSysEx8EndUmp = 0x3
	};



	enum WINDOWSMIDISERVICES_API Midi2MixedMessageStatus : MidiNibble4
	{
		Midi2MixedDataStatusHeader = 0x8,
		Midi2MixedDataStatusPayload = 0x9
	};
}

