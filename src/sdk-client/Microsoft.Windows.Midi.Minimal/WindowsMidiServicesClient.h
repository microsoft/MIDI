// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
//
// ------------------------------------------------------------------------------------------------

// ====================================================================
// Windows MIDI Services version 0.1.0
// For more information, please see https://github.com/microsoft/midi
// ====================================================================



// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.



// How to use the Windows MIDI Services API
// ===============================================================================================
// This has been developed using Visual Studio 2022 and C++ 20.
//
// If you are not able to consume C++ types, or are using an older version of the language spec,
// we recommend using the WinRT projections in the winmd. Those encapsulate the functionality 
// from this C++-specific version and add more cross-language support. 
//
// 1. Call the utility function to check that MIDI services are installed on this PC.
//    - If not installed, use the included functions to prompt the user to install
// 2. Create a new session
//    - you can have more than one session per app. Think of them as projects, 
//      tabs, etc.)
//    - Open devices/endpoints are scoped to the session and so have dedicated 
//      Send/Receive queues per endpoint per session
//    - Terminating the session cleans up all the open resources
// 3. Enumerate devices and endpoints
//    - Enumeration is not scoped to the session, but is global to the process
//    - Be sure to wire up callbacks for change notifications as well
// 4. Using the enumeration information, open one or more devices
// 5. Open one or more endpoints on those devices
// 6. Send/receive messages using UMPs or their strongly-typed derived types
//
// NOTES: 
// 
// MIDI CI
//		The Windows Service handles the MIDI CI discovery and protocol negotiation
//		tasks for the entire graph of connected devices and endpoints. Individual
//		applications do not need to send or handle those messages. 
//		Currently in discussion is how much of the rest of MIDI CI is handled
//		transparently by the OS.
// Jitter Reduction timestamps
//		Current thinking: Windows service handles creating outgoing Jitter 
//		reduction timestamps for endpoints which have negotiated (through MIDI
//		CI) to have JR timestamps sent. 
//
//		Incoming jitter reduction timestamps from other devices could be passed
//		through for applications to do with as they	please.
//		This can all result in a lot of incoming MIDI traffic, however (one
//		or more incoming JR timestamps per group, per channel, per endpoint 
//		per	250ms or less multiplied by the number of sessions handling incoming
//		MIDI messages from that endpoint). 
//		So the longer term plan is to automatically handle incoming JR 
//		timestamps and do a	running calculation in the service to quantify the 
//		jitter, and then update a property on the endpoint/group/channel with
//		the current	calculated incoming jitter. That's far less traffic going
//		through	another set of buffers and to the various client apps and sessions
//		which have that endpoint open.
//		Regardless, it is our hope that manufacturers following the	recommendation to
//		negotiate JR timestamps only for a single group when the different 
//		groups/channels would have the same jitter.

#pragma once

#include <Windows.h>
#include <string>
#include <filesystem>
#include <functional>
#include <map>

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

// TODO: May need to differentiate this namespace from the WinRT namespace

namespace Microsoft::Windows::Midi::Native
{
	// ----------------------------------------------------------------------------
	// Some potentially helpful things
	// ----------------------------------------------------------------------------

	namespace Utility
	{
		constexpr uint8_t Make7BitByte(uint8_t b) { return b & 0x7F; }

		constexpr uint16_t Make16BitManufacturerId(uint8_t b) { return (uint16_t)(b & 0x7F); }	// MSB is all zero, including high bit
		constexpr uint16_t Make16BitManufacturerId(uint8_t b1, uint8_t b2, uint8_t b3) { return (uint16_t)((b3 & 0x7F) | ((b2 & 0x7F) << 8) | 0x8000); }	// byte 1 is ignored, high bit is set high

		constexpr uint16_t Combine14BitsMsbLsb(uint8_t msb, uint8_t lsb) { return (uint16_t)((msb & 0x7F) << 8 | (lsb & 0x7F)); }

		constexpr uint8_t Msb(uint16_t number) { return (uint8_t)(number & 0xFF00 >> 8); }
		constexpr uint8_t Lsb(uint16_t number) { return (uint8_t)(number & 0x00FF); }

		constexpr uint16_t Msint16(uint32_t word) { return (uint16_t)(word & 0xFFFF0000 >> 16); }
		constexpr uint16_t Lsint16(uint32_t word) { return (uint16_t)(word & 0x0000FFFF); }

		// these are left to right with most significant = highest
		//constexpr uint8_t MostSignificantByte(uint32_t word) { return (uint16_t)((word & 0xFF000000) >> 24); }
		//constexpr uint8_t SecondMostSignificantByte(uint32_t word) { return (uint16_t)((word & 0x00FF0000) >> 16); }
		//constexpr uint8_t SecondLeastSignificantByte(uint32_t word) { return (uint16_t)((word & 0x0000FF00) >> 8); }
		//constexpr uint8_t LeastSignificantByte(uint32_t word) { return (uint16_t)(word & 0x000000FF); }

		constexpr uint8_t MostSignificantNibble(uint8_t b) { return (b & 0xF0) >> 4; }
		constexpr uint8_t LeastSignificantNibble(uint8_t b) { return b & 0x0F; }

		//constexpr uint8_t MostSignificantNibble(uint32_t word) { return (uint8_t)((word & 0xF0000000) >> 28); }			// example: message type
		//constexpr uint8_t SecondMostSignificantNibble(uint32_t word) { return (uint8_t)((word & 0x0F000000) >> 24); }	// example: group
	}

	// ============================================================================
	// Universal MIDI Packet (UMP)
	// 
	// The UMP is the base MIDI message type for this API. All messages, including
	// MIDI 1.0 messages, are packaged in UMPs. All functions which transmit or
	// receive messges work using UMPs. For devices which do not understand the UMP
	// the translation is handled by the USB class driver or other service-side
	// code.
	// 
	// The base UMP does not do any data validation or cleanup except in the 
	// provided (and optional-to-use) setters. If you already have valid MIDI UMP
	// validation and cleanup code in your apps, you may manipulate the words and
	// bytes directly.
	// 
	// A note on word/byte ordering:
	// -----------------------------
	// For the Windows implementation of UMP, Word[0] is the word with the first 
	// 4 bytes, including the message type and group nibbles in byte[0], and (when
	// used) channel and opcode in byte[1]
	// 
	// If you look at 2.1.1 in the UMP and MIDI 2.0 protocol specs document, this
	// follows the visuals in Figure 1.
	// ============================================================================

	// Base message structure
	struct WINDOWSMIDISERVICES_API Ump
	{
		virtual const uint8_t getMessageType() = 0;
		virtual const uint8_t getGroup() = 0;
		virtual void setGroup(const uint8_t value) = 0;
	};

	// 32 bit (1 word) MIDI message. Used for all MIDI 1.0 messages, utility messages, and more
	struct WINDOWSMIDISERVICES_API Ump32 : public Ump
	{
		union
		{
			uint32_t Word[1];
			uint8_t Byte[4];
		};

		virtual const uint8_t getMessageType();
		virtual const uint8_t getGroup();
		virtual void setGroup(const uint8_t value);
	};

	// 64 bit (2 words) MIDI message. Used for MIDI 2.0 channel voice, SysEx7, and more
	struct WINDOWSMIDISERVICES_API Ump64 : public Ump
	{
		union
		{
			uint32_t Word[2];
			uint8_t Byte[8];
		};

		virtual const uint8_t getMessageType();
		virtual const uint8_t getGroup();
		virtual void setGroup(const uint8_t value);
	};

	// 96 bit (3 words) MIDI message. Not currently used for any types of MIDI messages
	struct WINDOWSMIDISERVICES_API Ump96 : public Ump
	{
		union
		{
			uint32_t Word[3];
			uint8_t Byte[12];
		};

		virtual const uint8_t getMessageType();
		virtual const uint8_t getGroup();
		virtual void setGroup(const uint8_t value);

	};

	// 128 bit (4 words) MIDI message. Used for mixed data and 8-bit SysEx messages
	struct WINDOWSMIDISERVICES_API Ump128 : public Ump
	{
		union
		{
			uint32_t Word[4];
			uint8_t Byte[16];
		};

		virtual const uint8_t getMessageType();
		virtual const uint8_t getGroup();
		virtual void setGroup(const uint8_t value);
	};


	// ============================================================================
	// Strongly-typed MIDI Messages
	// You are not required to use the strongly typed messages. However, they can
	// aid in parsing and constructing messages, as well as translating from
	// MIDI 1.0 bytes to UMP messages. There are getter/setter functions for those
	// who prefer them and who want data validation/cleanup but the words and bytes 
	// themselves are directly available for manipulation without any validation
	// performed.
	// ============================================================================

	// ----------------------------------------------------------------------------
	// Strongly-typed messages : MIDI 1.0 Channel Voice
	// ----------------------------------------------------------------------------

	// Protocol spec 4.1. Base MIDI 1.0 channel voice message
	struct WINDOWSMIDISERVICES_API Midi1ChannelVoiceMessage : public Ump32
	{
		const uint8_t MessageType = 0x2;

		const uint8_t getOpcode() { return Utility::SecondMostSignificantNibble(Word[0]); };

		const uint8_t getChannel() { return Utility::MostSignificantNibble(Word[0]); };
		void setChannel(const uint8_t value);

	};

	// Protocol spec 4.1.1. MIDI 1.0 note off message
	struct WINDOWSMIDISERVICES_API Midi1NoteOffMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x8;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getVelocity();
		void setVelocity(const uint8_t value);

		static Midi1NoteOffMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte);
		static Midi1NoteOffMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t velocity);
	};

	// Protocol spec 4.1.2. MIDI 1.0 note on message
	struct WINDOWSMIDISERVICES_API Midi1NoteOnMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x9;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getVelocity();
		void setVelocity(const uint8_t value);

		static Midi1NoteOnMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte);
		static Midi1NoteOnMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t velocity);
	};

	// Protocol spec 4.1.3. MIDI 1.0 polyphonic pressure (aftertouch) message
	struct WINDOWSMIDISERVICES_API Midi1PolyPressureMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xA;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getData();
		void setData(const uint8_t value);

		static Midi1PolyPressureMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t dataByte);
		static Midi1PolyPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t data);
	};

	// Protocol spec 4.1.4. MIDI 1.0 control change message
	struct WINDOWSMIDISERVICES_API Midi1ControlChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xB;

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint8_t getData();
		void setData(const uint8_t value);

		static Midi1ControlChangeMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t indexByte, const uint8_t dataByte);
		static Midi1ControlChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t index, const uint8_t data);
	};

	// Protocol spec 4.1.5. MIDI 1.0 program change message
	struct WINDOWSMIDISERVICES_API Midi1ProgramChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xC;

		const uint8_t getProgram();
		void setProgram(const uint8_t value);

		static Midi1ProgramChangeMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t programByte);
		static Midi1ProgramChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t program);
	};

	// Protocol spec 4.1.6. MIDI 1.0 channel pressure (aftertouch) message
	struct WINDOWSMIDISERVICES_API Midi1ChannelPressureMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xD;

		const uint8_t getData();
		void setData(const uint8_t value);

		static Midi1ChannelPressureMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t dataByte);
		static Midi1ChannelPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t data);
	};

	// Protocol spec 4.1.7. MIDI 1.0 pitch bend message
	struct WINDOWSMIDISERVICES_API Midi1PitchBendMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xE;

		const uint8_t getDataLsb();
		void setDataLsb(const uint8_t value);

		const uint8_t getDataMsb();
		void setDataMsb(const uint8_t value);

		const uint16_t getDataCombined();
		void setDataCombined(const uint16_t value);

		static Midi1PitchBendMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t lsbDataByte, const uint8_t msbDataByte);
		static Midi1PitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t dataLsb, const uint8_t dataMsb);
		static Midi1PitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint16_t data);
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : MIDI 2.0 Channel Voice
	// ----------------------------------------------------------------------------


	// Protocol spec 4.2. MIDI 2.0 base channel voice message
	struct WINDOWSMIDISERVICES_API Midi2ChannelVoiceMessage : public Ump64
	{
		const uint8_t MessageType = 0x4;

		const uint8_t getChannel() { return Utility::MostSignificantNibble(Word[0]); };
		void setChannel(const uint8_t value);
	};

	// Protocol spec 4.2.1. MIDI 2.0 note off message. See notes on MIDI 2.0 note attributes
	struct WINDOWSMIDISERVICES_API Midi2NoteOffMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x8;

		//conversion method. Uses MIDI 1.0 rules
		static Midi2NoteOffMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte);
		static Midi2NoteOffMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t attributeType, const uint16_t velocity, const uint16_t attributeData);

		// TODO fill this out
	};

	// Protocol spec 4.2.2. MIDI 2.0 note on message. See notes on MIDI 2.0 note attributes
	struct WINDOWSMIDISERVICES_API Midi2NoteOnMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x9;

		//conversion method. Uses MIDI 1.0 rules
		static Midi2NoteOnMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte);
		static Midi2NoteOnMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t attributeType, const uint16_t velocity, const uint16_t attributeData);

		// TODO fill this out
	};

	// Protocol spec 4.2.3. MIDI 2.0 polyphonic pressure (aftertouch) message.
	struct WINDOWSMIDISERVICES_API Midi2PolyPressureMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xA;

		static Midi2PolyPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint32_t data);


		// TODO fill this out
	};

	// Protocol spec 4.2.4. MIDI 2.0 registered per-note controller message.
	struct WINDOWSMIDISERVICES_API Midi2RegisteredPerNoteControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x0;

		static Midi2RegisteredPerNoteControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t index, const uint32_t data);

		// TODO fill this out
	};

	// Protocol spec 4.2.4. MIDI 2.0 assignable per-note controller message.
	struct WINDOWSMIDISERVICES_API Midi2AssignablePerNoteControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x1;

		static Midi2AssignablePerNoteControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t index, const uint32_t data);

		// TODO fill this out
	};

	// Protocol spec 4.2.5. MIDI 2.0 per-note management message.
	struct WINDOWSMIDISERVICES_API Midi2PerNoteManagementMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xF;

		static Midi2AssignablePerNoteControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t optionFlags);


		// TODO fill this out
	};

	// Protocol spec 4.2.6. MIDI 2.0 control change message.
	struct WINDOWSMIDISERVICES_API Midi2ControlChangeMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xB;

		static Midi2ControlChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t index, const uint32_t data);


		// TODO fill this out
	};

	// Protocol spec 4.2.7. MIDI 2.0 registered (RPN) controller message.
	struct WINDOWSMIDISERVICES_API Midi2RegisteredControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x2;

		static Midi2RegisteredControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);


		// TODO fill this out
	};

	// Protocol spec 4.2.7. MIDI 2.0 assignable (NRPN) controller message.
	struct WINDOWSMIDISERVICES_API Midi2AssignableControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x3;

		static Midi2AssignableControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);


		// TODO fill this out
	};

	// Protocol spec 4.2.8. MIDI 2.0 relative registered (R-RPN) controller message.
	struct WINDOWSMIDISERVICES_API Midi2RelativeRegisteredControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x4;

		static Midi2RelativeRegisteredControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);


		// TODO fill this out
	};

	// Protocol spec 4.2.8. MIDI 2.0 relative assignable (R-NRPN) controller message.
	struct WINDOWSMIDISERVICES_API Midi2RelativeAssignableControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x5;

		static Midi2RelativeAssignableControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);


		// TODO fill this out
	};

	// Protocol spec 4.2.9. MIDI 2.0 program change message.
	struct WINDOWSMIDISERVICES_API Midi2ProgramChangeMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xC;

		static Midi2ProgramChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t optionFlags, const uint8_t program, const uint8_t bankMsb, const uint8_t bankLsb);

		// TODO fill this out
	};

	// Protocol spec 4.2.10. MIDI 2.0 channel pressure (aftertouch) message.
	struct WINDOWSMIDISERVICES_API Midi2ChannelPressureMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xD;

		static Midi2ChannelPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint32_t data);

		// TODO fill this out
	};

	// Protocol spec 4.2.11. MIDI 2.0 channel pitch bend message.
	struct WINDOWSMIDISERVICES_API Midi2PitchBendMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xE;

		static Midi2PitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint32_t data);

		// TODO fill this out
	};

	// Protocol spec 4.2.12. MIDI 2.0 per-note pitch bend message.
	struct WINDOWSMIDISERVICES_API Midi2PerNotePitchBendMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x6;

		static Midi2PerNotePitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint32_t data);

		// TODO fill this out
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : Data
	// ----------------------------------------------------------------------------


	// Status nibble for a 7 bit sysex message
	enum WINDOWSMIDISERVICES_API SystemExclusive7MessageStatus
	{
		CompleteInOneUmp = 0x0,
		StartUmp = 0x1,
		ContinueUmp = 0x2,
		EndUmp = 0x3
	};


	// MIDI 7-bit MIDI 1.0-data-compatible System Exclusive message
	// See notes in spec as this is not identical to a MIDI 1.0 sysex stream
	struct WINDOWSMIDISERVICES_API MidiSystemExclusive7BitMessage final : public Ump64
	{
		const uint8_t MessageType = 0x3;

		const SystemExclusive7MessageStatus getStatus();
		void setStatus(const SystemExclusive7MessageStatus value);

		const uint8_t getNumBytes();
		void setNumBytes(const uint8_t value);

		// TODO methods to get and set up to 6 bytes of data and also factory methods
	};

	// Status nibble for an 8 bit sysex message
	enum WINDOWSMIDISERVICES_API SystemExclusive8MessageStatus
	{
		CompleteInOneUmp = 0x0,
		StartUmp = 0x1,
		ContinueUmp = 0x2,
		EndUmp = 0x3
	};

	// MIDI 2 8-bit system exclusive message. Recommended for new implementations
	// which don't require 7-bit MIDI 1.0 compatibility. Using this message can
	// cut down on total bytes transferred by using all 8 bits of each data byte.
	struct WINDOWSMIDISERVICES_API Midi2SystemExclusive8BitMessage final : public Ump128
	{
		const uint8_t MessageType = 0x5;

		const SystemExclusive8MessageStatus getStatus();
		void setStatus(const SystemExclusive8MessageStatus value);

		const uint8_t getNumBytes();
		void setNumBytes(const uint8_t value);

		const uint8_t getStreamId();
		void setStreamId(const uint8_t value);

		// TODO methods to get and set up to 13 bytes of data and also factory methods
	};

	// MIDI 2.0 mixed data set header message. 
	struct WINDOWSMIDISERVICES_API MidiMixedDataSetHeaderMessage final : public Ump128
	{
		// TODO fill this out
	};
	
	// MIDI 2.0 mixed data set payload message. 
	struct WINDOWSMIDISERVICES_API MidiMixedDataSetPayloadMessage final : public Ump128
	{
		// TODO fill this out
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : System
	// ----------------------------------------------------------------------------


	struct WINDOWSMIDISERVICES_API MidiSystemMessage : public Ump32
	{
		const uint8_t MessageType = 0x1;
	};

	struct WINDOWSMIDISERVICES_API MidiTimeCodeMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xF1;

		static MidiTimeCodeMessage FromValues(const uint8_t group, const uint8_t timecodeByte);
	};

	struct WINDOWSMIDISERVICES_API MidiSongPositionPointerMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xF2;

		// note: lsb is before msb per MIDI 1.0

		const uint8_t getDataLsb();
		void setDataLsb(const uint8_t value);

		const uint8_t getDataMsb();
		void setDataMsb(const uint8_t value);


		static MidiSongPositionPointerMessage FromValues(const uint8_t group, const uint8_t positionLsbByte, const uint8_t positionMsbByte);
	};

	struct WINDOWSMIDISERVICES_API MidiSongSelectMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xF3;

		const uint8_t getSong();
		void setSong(const uint8_t value);

		static MidiSongSelectMessage FromValues(const uint8_t group, const uint8_t songByte);
	};

	struct WINDOWSMIDISERVICES_API MidiTuneRequestMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xF6;

		static MidiTuneRequestMessage FromValues(const uint8_t group);
	};

	struct WINDOWSMIDISERVICES_API MidiTimingClockMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xF8;

		static MidiTimingClockMessage FromValues(const uint8_t group);
	};

	struct WINDOWSMIDISERVICES_API MidiStartMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xFA;

		static MidiStartMessage FromValues(const uint8_t group);
	};

	struct WINDOWSMIDISERVICES_API MidiContinueMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xFB;

		static MidiContinueMessage FromValues(const uint8_t group);
	};

	struct WINDOWSMIDISERVICES_API MidiStopMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xFC;

		static MidiStopMessage FromMidi1Bytes(const uint8_t group);
	};

	struct WINDOWSMIDISERVICES_API MidiActiveSensingMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xFE;

		static MidiActiveSensingMessage FromMidi1Bytes(const uint8_t group);
	};

	struct WINDOWSMIDISERVICES_API MidiResetMessage final : public MidiSystemMessage
	{
		const uint8_t Status = 0xFF;

		static MidiResetMessage FromMidi1Bytes(const uint8_t group);
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : Utility
	// ----------------------------------------------------------------------------


	struct WINDOWSMIDISERVICES_API Midi2UtilityMessage : public Ump32
	{
		const uint8_t MessageType = 0x0;
	};

	struct WINDOWSMIDISERVICES_API Midi2JitterReductionClockMessage final : public Midi2UtilityMessage
	{
		const uint8_t Status = 0x01;

		const uint16_t getClockTime();
		void setClockTime(const uint16_t value);
	
		static Midi2JitterReductionClockMessage FromValues(const uint8_t group, const uint16_t clockTime);

		// TODO fill this out
	};

	struct WINDOWSMIDISERVICES_API Midi2JitterReductionTimestampMessage final : public Midi2UtilityMessage
	{
		const uint8_t Status = 0x02;

		const uint16_t getTimestamp();
		void setTimestamp(const uint16_t value);

		static Midi2JitterReductionTimestampMessage FromValues(const uint8_t group, const uint16_t timestamp);

		// TODO fill this out
	};

	struct WINDOWSMIDISERVICES_API Midi2NoopMessage final : public Midi2UtilityMessage
	{
		// TODO fill this out
	};


	// ----------------------------------------------------------------------------
	// MIDI CI
	// ----------------------------------------------------------------------------




	// TODO *****************************



	// ----------------------------------------------------------------------------
	// Enumeration
	// ----------------------------------------------------------------------------


	// GUIDs are used for IDs. If porting this to another platform, consider
	// usig something like CrossGuid, taking an API dependency on boost, or
	// simply redefining as necessary
	// https://github.com/graeme-hill/crossguid

	using MidiObjectId = GUID ;

	// examples: USB, BLE, RTP
	struct WINDOWSMIDISERVICES_API MidiTransportInformation
	{
		MidiObjectId Id;					// Unique Id of the type of transport. Referenced by the device
		std::string Name;					// Name, like BLE, RTP, USB etc.
		std::string LongName;				// Longer name like Bluetooth Low Energy MIDI 1.0
		std::string IconFileName;			// Name, without path, of the image used to represent this type of transport
		std::string Description;			// Text description of the transport. 
	};

	// examples: Some synth, some controller
	struct WINDOWSMIDISERVICES_API MidiDeviceInformation
	{
		MidiObjectId Id;					// Unique Id of the device. Used in most MIDI messaging
		MidiObjectId TransportId;			// Uinque Id of the transport used by the device. For displaying appropriate name/icons
		std::string Name;					// Device name. May have been changed by the user through config tools
		std::string DeviceSuppliedName;		// Device name as supplied by the device plug-in or driver
		std::string Serial;					// If there's a unique serial number for the device, we track it here.
		std::string IconFileName;			// Name, without path, of the image used to represent this specific device
		std::string Description;			// Text description of the device. 
	};

	enum WINDOWSMIDISERVICES_API MidiEndpointType
	{
		MidiOut = 0,
		MidiIn = 1,
		Bidirectional = 2
	};

	// for USB-connected single devices, an endpoint is typically synonomous
	// with the device but for devices which provide other endpoints (like a
	// synth with multiple DIN MIDI ports, or other USB or network ports), 
	// device:endpoint relationship is a 1:1 to 1:many relationship
	struct WINDOWSMIDISERVICES_API MidiEndpointInformation final
	{
		MidiObjectId Id;					// Unique Id of the endpoint. Used in most MIDI messaging
		MidiObjectId ParentDeviceId;		// Unique Id of the parent device which owns this endpoint.
		MidiEndpointType EndpointType;		// Type of endpoint. Mostly used to differentiate unidirectional (like DIN) from bidirectional streams
		std::string Name;					// Name of this endpoint. May have been changed by the user through config tools.
		std::string DeviceSuppliedName;		// Endpoint name as supplied by the device plug-in or driver
		std::string IconFileName;			// Name, without path, of the image used to represent this specific endpoint
		std::string Description;			// Text description of the endpoint.
	};

	// ----------------------------------------------------------------------------
	// Enumeration change callbacks / delegates
	// ----------------------------------------
	// In previous versions of Windows MIDI APIs, like WinMM or WinRT MIDI, devices
	// and ports could be added or removed, but generally did not otherwise change. 
	// In this version, and in MIDI 2.0 in general, devices, endpoints, and more
	// can change properties at any time. Those changes may be due to MIDI CI
	// notifications, or user action. 
	// 
	// We encourage developers to track when devices/endpoints are added or removed, 
	// or when properties of those devices/endpoints/etc change. 
	// 
	// The API objects themselves will be automatically updated as a result of 
	// these change notifications.
	// ----------------------------------------------------------------------------

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiTransportInformation& information)> MidiTransportAddedCallback;

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiTransportInformation& information)> MidiTransportRemovedCallback;

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiTransportInformation& oldInformation,
		const MidiTransportInformation& newInformation)> MidiTransportChangedCallback;


	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiDeviceInformation& information)> MidiDeviceAddedCallback;

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiDeviceInformation& information)> MidiDeviceRemovedCallback;

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiDeviceInformation& oldInformation,
		const MidiDeviceInformation& newInformation)> MidiDeviceChangedCallback;


	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiEndpointInformation& fnformation)> MidiEndpointAddedCallback;

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiEndpointInformation& information)> MidiEndpointRemovedCallback;

	typedef WINDOWSMIDISERVICES_API std::function<void(
		const MidiEndpointInformation& oldInformation,
		const MidiEndpointInformation& newInformation)> MidiEndpointChangedCallback;



	// Enumerator class. Responsible for exposing information about every device
	// and endpoint known to the system.
	class WINDOWSMIDISERVICES_API MidiEnumerator final
	{
	private:
		// device id, device info
		std::map<MidiObjectId, MidiTransportInformation> _transports;

		// device id, device info
		std::map<MidiObjectId, MidiDeviceInformation> _devices;

		// device id, endpoint id, endpoint info
		std::map<std::pair<MidiObjectId, MidiObjectId>, MidiEndpointInformation> _endpoints;

		// TODO: Will need to provide a hash function for the above. 
		// Could use boost hash_combine or hash_value for std::pair
		// old example: https://stackoverflow.com/questions/32685540/why-cant-i-compile-an-unordered-map-with-a-pair-as-key

	public:
		void Load();

		const MidiTransportInformation& GetTransportInformation(MidiObjectId transportId);
		const MidiDeviceInformation& GetDeviceInformation(MidiObjectId deviceId);
		const MidiDeviceInformation& GetEndpointInformation(MidiObjectId deviceId, MidiObjectId endpointId);


		void SubscribeToTransportChangeNotifications(const MidiTransportAddedCallback& addedCallback, const MidiTransportRemovedCallback& removedCallback, const MidiTransportChangedCallback& changedCallback);
		void SubscribeToDeviceChangeNotifications(const MidiDeviceAddedCallback& addedCallback, const MidiDeviceRemovedCallback& removedCallback, const MidiDeviceChangedCallback& changedCallback);
		void SubscribeToEndpointChangeNotifications(const MidiEndpointAddedCallback& addedCallback, const MidiEndpointRemovedCallback& removedCallback, const MidiEndpointChangedCallback& changedCallback);
	};


	// ----------------------------------------------------------------------------
	// MIDI Message received callback / delegate
	// ----------------------------------------------------------------------------

	// We may try a few different approaches here with the message buffer, to try to reduce
	// data copies and maximize performance
	typedef WINDOWSMIDISERVICES_API std::function<void(
						const MidiObjectId& sessionId,
						const MidiObjectId& deviceId,
						const MidiObjectId& endpointId,
						std::vector<Ump> messages)> MidiMessagesReceivedCallback;


	// ----------------------------------------------------------------------------
	// Devices and Endpoints
	// ----------------------
	// Not to be confused with EndpointInformation and DeviceInformation, these
	// are the actual interfaces to the devices and endpoints themselves.
	// An endpoint is conceptually the same as a Port or Port pair in MIDI 1.0,
	// with a big distinction: it supports bidirectional communication for
	// MIDI CI and MIDI 2.0, when it is available.
	// Classic MIDI 1.0 InputPort and OutputPort types can still be used by
	// opening them with the appropriate MidiEndpointOpenOptions value.
	// 
	// Note: Currently, there is no provision in this API for taking a classic 
	// MIDI 1.0 output and input and combining them to be treated as a single
	// bidirectional port. We're investigating putting that in the service itself
	// and allowing the user to do that pairing, should they want to.
	// ----------------------------------------------------------------------------


	enum WINDOWSMIDISERVICES_API MidiEndpointOpenOptions
	{
		OpenOutput = 1,				// MIDI Out
		OpenInput = 2,				// MIDI In, will need to wire up message received handler
		OpenBidirectional = 3		// Both. Will need to wire up message received handler
	};

	class WINDOWSMIDISERVICES_API MidiEndpoint final
	{
	private:
		MidiEndpointInformation Information;
		MidiMessagesReceivedCallback _messagesReceivedCallback;

		MidiEndpoint(const MidiEndpointInformation& information, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiEndpoint(const MidiEndpointInformation& information);

		// TODO: Vector of groups / channels / function blocks / protocol versions / other MIDI CI information

	public:

		const MidiEndpointInformation getInformation();

		friend class MidiDevice;	// TBD if this is necessary. Want to construct endpoints only through device class

	};

	struct WINDOWSMIDISERVICES_API MidiDeviceOpenOptions 
	{
		// TODO
	};

	class WINDOWSMIDISERVICES_API MidiDevice final
	{
	private:
		MidiDeviceInformation Information;

		std::vector<MidiEndpoint> _openEndpoints;

		MidiDevice(MidiDeviceInformation information);

	public:
		const MidiDeviceInformation getInformation();
		const std::vector<MidiEndpoint> getAllOpenEndpoints();
		const MidiEndpoint getOpenEndpoint(const MidiObjectId& endpointId);

		// session sets these when you open the device
		const GUID getParentSessionID();
		void setParentSessionID(const MidiObjectId sessionId);


		MidiEndpoint OpenEndpoint(const MidiEndpointInformation& endpointInformation, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiEndpoint OpenEndpoint(const MidiEndpointInformation& endpointInformation, const MidiEndpointOpenOptions options);

		MidiEndpoint OpenEndpoint(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiEndpoint OpenEndpoint(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options);

		// maybe these should live in the endpoint class instead of here
		bool SendUmp(const MidiEndpointInformation& information, const Ump& message);
		bool SendUmp(const MidiObjectId& endpointId, const Ump& message);

		friend class MidiSession;	// TBD if this is necessary. Want to construct devices only through session class

	};




	// ----------------------------------------------------------------------------
	// Session
	// ----------------------------------------------------------------------------

	struct WINDOWSMIDISERVICES_API MidiSessionCreateOptions
	{

	};

	class WINDOWSMIDISERVICES_API MidiSession final
	{
	private:
		MidiObjectId _id;
		std::string _name;
		SYSTEMTIME _createdDateTime;

		std::vector<MidiDevice> _openDevices;

	public:
		static MidiSession Create(const std::string& name, const std::string& appName, const MidiSessionCreateOptions& options);
		static MidiSession Create(const std::string& name, const std::string& appName);

		const std::vector<MidiDevice> getAllOpenDevices();
		const MidiDevice getOpenDevice(MidiObjectId id);


		MidiDevice OpenDevice(const MidiObjectId& deviceId, const MidiDeviceOpenOptions& options);
		MidiDevice OpenDevice(const MidiObjectId& deviceId);


		// TODO: Events/callbacks for message receive

		// TODO: vector of open devices/endpoints

		// TODO: Need to decide if the JR timestamps are handled internally and 
		// automatically. Needs CI negotiation and the right clock source.
	//	void SendUmpWithJRTimestamp(MidiEndpointInformation information, MidiJitterReductionTimestampMessage timestamp, Ump message);
	//	void SendUmpWithJRTimestamp(GUID deviceId, GUID endpointId, MidiJitterReductionTimestampMessage timestamp, Ump message);

		MidiObjectId GetId();
		SYSTEMTIME GetCreatedDateTime();

		const std::string GetName();
		void UpdateName(const std::string& newName);		// this makes a server call
	};


	// ----------------------------------------------------------------------------
	// Service-specific information and configuration
	// ----------------------------------------------------------------------------


	enum WINDOWSMIDISERVICES_API MidiServicePingResponse
	{
		Pong,
		TimeOut,
		Error
	};


	struct WINDOWSMIDISERVICES_API MidiServicesComponentVersion
	{
		int32_t Major;
		int32_t Minor;
		int32_t Build;
		int32_t Revision;
	};



	class WINDOWSMIDISERVICES_API MidiService final
	{
	public:

		static MidiServicesComponentVersion getClientApiVersion;

		// returns true if the Windows MIDI Services service is installed
		static bool getIsInstalled();

		// returns true if the SCM reports that the services are running
		// It doesn't tell you if it's functioning properly
		static bool getIsRunning();

		// start the MIDI Service if it's not running. Should be unnecessary in most cases
		static bool Start();		

		// pings the service. This will tell you if it is processing messages.
		static MidiServicePingResponse Ping(MidiServicesComponentVersion& serverVersionReported);

		static const std::string getServicesInstallerUri();

		// Returns the full file name, including the path, for an icon. Useful
		// for apps which want to show the icon in the app
		static const std::filesystem::path BuildFullPathDeviceIconFilename(const std::string& iconFileName);
		static const std::filesystem::path BuildFullPathEndpointIconFilename(const std::string& iconFileName);
		static const std::filesystem::path BuildFullPathTransportIconFilename(const std::string& iconFileName);
	};
}