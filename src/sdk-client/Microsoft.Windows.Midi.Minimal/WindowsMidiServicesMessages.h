// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

// ============================================================================
// Strongly-typed MIDI Messages
// You are not required to use the strongly typed messages. However, they can
// aid in parsing and constructing messages, as well as translating from
// MIDI 1.0 bytes to UMP messages. There are getter/setter functions for those
// who prefer them and who want data validation/cleanup but the words and bytes 
// themselves are directly available for manipulation without any validation
// performed.
// ============================================================================


#pragma once

#include <stdint.h>


#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include "WindowsMidiServicesUmp.h"

namespace Microsoft::Windows::Midi::Messages
{

	// ----------------------------------------------------------------------------
	// Strongly-typed messages : MIDI 1.0 Channel Voice
	// ----------------------------------------------------------------------------

	// Protocol spec 4.1. Base MIDI 1.0 channel voice message
	// Byte[0] : Message Type and Group
	// Byte[1] : Opcode and Channel
	struct WINDOWSMIDISERVICES_API Midi1ChannelVoiceMessage : public Ump32
	{
		const uint8_t MessageType = 0x2;

		const uint8_t getOpcode();

		const uint8_t getChannel();
		void setChannel(const uint8_t value);

	};

	// Protocol spec 4.1.1. MIDI 1.0 note off message
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Velocity (7 bits)
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
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Velocity (7 bits)
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
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Data (7 bits)
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
	// Byte[2] : Index (7 bits)
	// Byte[3] : Data (7 bits)
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
	// Byte[2] : Program (7 bits)
	// Byte[3] : (unused: set to zero)
	struct WINDOWSMIDISERVICES_API Midi1ProgramChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xC;

		const uint8_t getProgram();
		void setProgram(const uint8_t value);

		static Midi1ProgramChangeMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t programByte);
		static Midi1ProgramChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t program);
	};

	// Protocol spec 4.1.6. MIDI 1.0 channel pressure (aftertouch) message
	// Byte[2] : Data (7 bits)
	// Byte[3] : (unused: set to zero)
	struct WINDOWSMIDISERVICES_API Midi1ChannelPressureMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xD;

		const uint8_t getData();
		void setData(const uint8_t value);

		static Midi1ChannelPressureMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t dataByte);
		static Midi1ChannelPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t data);
	};

	// Protocol spec 4.1.7. MIDI 1.0 pitch bend message
	// Byte[2] : Data LSB (7 bits)
	// Byte[3] : Data MSB (7 bits)
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
	// Byte[0] : Message Type and Group
	// Byte[1] : Opcode and Channel
	struct WINDOWSMIDISERVICES_API Midi2ChannelVoiceMessage : public Ump64
	{
		const uint8_t MessageType = 0x4;

		const uint8_t getOpcode();

		const uint8_t getChannel();
		void setChannel(const uint8_t value);

	};

	// Protocol spec 4.2.1. MIDI 2.0 note off message. See notes on MIDI 2.0 note attributes
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Attribute Type (8 bits)
	// Short[2] : Velocity (16 bits)
	// Short[3] : Attribute Data (16 bits)
	struct WINDOWSMIDISERVICES_API Midi2NoteOffMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x8;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getAttributeType();
		void setAttributeType(const uint8_t value);

		const uint16_t getVelocity();
		void setVelocity(const uint16_t value);

		const uint16_t getAttributeData();
		void setAttributeData(const uint16_t value);

		//conversion method. Uses MIDI 1.0 rules
		static Midi2NoteOffMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte);
		static Midi2NoteOffMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t attributeType, const uint16_t velocity, const uint16_t attributeData);
	};

	// Protocol spec 4.2.2. MIDI 2.0 note on message. See notes on MIDI 2.0 note attributes
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Attribute Type (8 bits)
	// Short[2] : Velocity (16 bits)
	// Short[3] : Attribute Data (16 bits)
	struct WINDOWSMIDISERVICES_API Midi2NoteOnMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x9;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getAttributeType();
		void setAttributeType(const uint8_t value);

		const uint16_t getVelocity();
		void setVelocity(const uint16_t value);

		const uint16_t getAttributeData();
		void setAttributeData(const uint16_t value);

		//conversion method. Uses MIDI 1.0 rules
		static Midi2NoteOnMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte);
		static Midi2NoteOnMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t attributeType, const uint16_t velocity, const uint16_t attributeData);
	};

	// Protocol spec 4.2.3. MIDI 2.0 polyphonic pressure (aftertouch) message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PolyPressureMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xA;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);


		static Midi2PolyPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint32_t data);
	};

	// Protocol spec 4.2.4. MIDI 2.0 registered per-note controller message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Index (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RegisteredPerNoteControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x0;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);

		static Midi2RegisteredPerNoteControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.4. MIDI 2.0 assignable per-note controller message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Index (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2AssignablePerNoteControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x1;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);


		static Midi2AssignablePerNoteControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.5. MIDI 2.0 per-note management message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Option flags (8 bits)
	// Word[2] : (unused. Set to zero) (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PerNoteManagementMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xF;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getOptionFlags();
		void setOptionFlags(const uint8_t value);

	
		static Midi2PerNoteManagementMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t optionFlags);
	};

	// Protocol spec 4.2.6. MIDI 2.0 control change message.
	// Byte[2] : Index (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data(32 bits)
	struct WINDOWSMIDISERVICES_API Midi2ControlChangeMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xB;

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);

		static Midi2ControlChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.7. MIDI 2.0 registered (RPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RegisteredControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x2;

		const uint8_t getBank();
		void setBank(const uint8_t value);

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);
	
		static Midi2RegisteredControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.7. MIDI 2.0 assignable (NRPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2AssignableControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x3;

		const uint8_t getBank();
		void setBank(const uint8_t value);

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);

		static Midi2AssignableControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.8. MIDI 2.0 relative registered (R-RPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RelativeRegisteredControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x4;

		const uint8_t getBank();
		void setBank(const uint8_t value);

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);

		static Midi2RelativeRegisteredControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.8. MIDI 2.0 relative assignable (R-NRPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RelativeAssignableControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x5;

		const uint8_t getBank();
		void setBank(const uint8_t value);

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);
	
		static Midi2RelativeAssignableControllerMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t bank, const uint8_t index, const uint32_t data);
	};

	// Protocol spec 4.2.9. MIDI 2.0 program change message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : Option flags (8 bits)
	// Byte[4] : Program (7 bits)
	// Byte[5] : (unused. Set to zero) (8 bits)
	// Byte[6] : Bank MSB (7 bits)
	// Byte[7] : Bank LSB (7 bits)
	struct WINDOWSMIDISERVICES_API Midi2ProgramChangeMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xC;

		const uint8_t getOptionFlags();
		void setOptionFlags(const uint8_t value);

		const uint8_t getProgram();
		void setProgram(const uint8_t value);

		const uint8_t getBankMsb();
		void setBankMsb(const uint8_t value);

		const uint8_t getBankLsb();
		void setBankLsb(const uint8_t value);


		static Midi2ProgramChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t optionFlags, const uint8_t program, const uint8_t bankMsb, const uint8_t bankLsb);
	};

	// Protocol spec 4.2.10. MIDI 2.0 channel pressure (aftertouch) message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2ChannelPressureMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xD;

		const uint32_t getData();
		void setData(const uint32_t value);
	
		static Midi2ChannelPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint32_t data);
	};

	// Protocol spec 4.2.11. MIDI 2.0 channel pitch bend message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PitchBendMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xE;

		const uint32_t getData();
		void setData(const uint32_t value);
	
		static Midi2PitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint32_t data);
	};

	// Protocol spec 4.2.12. MIDI 2.0 per-note pitch bend message.
	// Byte[2] : Note number (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PerNotePitchBendMessage final : public Midi2ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x6;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint32_t getData();
		void setData(const uint32_t value);
	
		static Midi2PerNotePitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint32_t data);
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : Data
	// ----------------------------------------------------------------------------


	// Status nibble for a 7 bit sysex message
	enum WINDOWSMIDISERVICES_API SystemExclusive7MessageStatus
	{
		SysEx7CompleteInOneUmp = 0x0,
		SysEx7StartUmp = 0x1,
		SysEx7ContinueUmp = 0x2,
		SysEx7EndUmp = 0x3
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
		SysEx8CompleteInOneUmp = 0x0,
		SysEx8StartUmp = 0x1,
		SysEx8ContinueUmp = 0x2,
		SysEx8EndUmp = 0x3
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




}