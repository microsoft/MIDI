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

// NOTE TO Team: If we break things up into in-box and out of box, this header file
// and its related library could easily be a nuget package or out of band library 
// which sits on top of the UMP definitions

#pragma once

#include <stdint.h>


#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include "WindowsMidiServicesUmp.h"

namespace Microsoft::Windows::Midi::Messages //::inline v0_1_0_pre
{

	// ----------------------------------------------------------------------------
	// Strongly-typed messages : MIDI 1.0 Channel Voice
	// For details of the values in MIDI 1.0 messages, please see:
	// https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
	// ----------------------------------------------------------------------------

	// Protocol spec 4.1. Base MIDI 1.0 channel voice message
	// Byte[0] : Message Type and Group
	// Byte[1] : Opcode and Channel
	struct WINDOWSMIDISERVICES_API Midi1ChannelVoiceMessage : public Ump32
	{
		const MidiMessageType MessageType = MidiMessageTypeMidi1ChannelVoice;

		const MidiNibble4 getOpcode();

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

		const MidiChannel getChannel();
		void setChannel(const MidiChannel value);

	};

	// Protocol spec 4.1.1. MIDI 1.0 note off message
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Velocity (7 bits)
	struct WINDOWSMIDISERVICES_API Midi1NoteOffMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodeNoteOff;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiByte7 getVelocity();
		void setVelocity(const MidiByte7 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1NoteOffMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 noteNumberByte,
			const MidiByte7 velocityByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1NoteOffMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiByte7 velocity);
	};

	// Protocol spec 4.1.2. MIDI 1.0 note on message
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Velocity (7 bits)
	struct WINDOWSMIDISERVICES_API Midi1NoteOnMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodeNoteOn;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiByte7 getVelocity();
		void setVelocity(const MidiByte7 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1NoteOnMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 noteNumberByte,
			const MidiByte7 velocityByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1NoteOnMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiByte7 velocity);
	};

	// Protocol spec 4.1.3. MIDI 1.0 polyphonic pressure (aftertouch) message
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Data (7 bits)
	struct WINDOWSMIDISERVICES_API Midi1PolyPressureMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodePolyPressure;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiByte7 getData();
		void setData(const MidiByte7 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1PolyPressureMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 noteNumberByte,
			const MidiByte7 dataByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1PolyPressureMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiByte7 data);
	};

	// Protocol spec 4.1.4. MIDI 1.0 control change message
	// Byte[2] : Index (7 bits)
	// Byte[3] : Data (7 bits)
	struct WINDOWSMIDISERVICES_API Midi1ControlChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodeControlChange;

		const MidiByte7 getIndex();
		void setIndex(const MidiByte7 value);

		const MidiByte7 getData();
		void setData(const MidiByte7 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1ControlChangeMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 indexByte,
			const MidiByte7 dataByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1ControlChangeMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 index,
			const MidiByte7 data);
	};

	// Protocol spec 4.1.5. MIDI 1.0 program change message
	// Byte[2] : Program (7 bits)
	// Byte[3] : (unused: set to zero)
	struct WINDOWSMIDISERVICES_API Midi1ProgramChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodeProgramChange;

		const MidiByte7 getProgram();
		void setProgram(const MidiByte7 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1ProgramChangeMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 programByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1ProgramChangeMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 program);
	};

	// Protocol spec 4.1.6. MIDI 1.0 channel pressure (aftertouch) message
	// Byte[2] : Data (7 bits)
	// Byte[3] : (unused: set to zero)
	struct WINDOWSMIDISERVICES_API Midi1ChannelPressureMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodeChannelPressure;

		const MidiByte7 getData();
		void setData(const MidiByte7 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1ChannelPressureMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 dataByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1ChannelPressureMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 data);
	};

	// Protocol spec 4.1.7. MIDI 1.0 pitch bend message
	// Byte[2] : Data LSB (7 bits)
	// Byte[3] : Data MSB (7 bits)
	struct WINDOWSMIDISERVICES_API Midi1PitchBendMessage final : public Midi1ChannelVoiceMessage
	{
		const Midi1ChannelVoiceOpcode Opcode = Midi1ChannelVoiceOpcodePitchBend;

		const MidiByte7 getDataLsb();
		void setDataLsb(const MidiByte7 value);

		const MidiByte7 getDataMsb();
		void setDataMsb(const MidiByte7 value);

		const MidiShort14 getDataCombined();
		void setDataCombined(const MidiShort14 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi1PitchBendMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 lsbDataByte,
			const MidiByte7 msbDataByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1PitchBendMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 dataLsb,
			const MidiByte7 dataMsb);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi1PitchBendMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const uint16_t data);
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : MIDI 2.0 Channel Voice
	// ----------------------------------------------------------------------------


	// Protocol spec 4.2. MIDI 2.0 base channel voice message
	// Byte[0] : Message Type and Group
	// Byte[1] : Opcode and Channel
	struct WINDOWSMIDISERVICES_API Midi2ChannelVoiceMessage : public Ump64
	{
		const MidiMessageType MessageType = MidiMessageTypeMidi2ChannelVoice;

		const MidiNibble4 getOpcode();

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

		const MidiChannel getChannel();
		void setChannel(const MidiChannel value);
	};



	// Protocol spec 4.2.1. MIDI 2.0 note off message. See notes on MIDI 2.0 note attributes
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Attribute Type (8 bits)
	// Short[2] : Velocity (16 bits)
	// Short[3] : Attribute Data (16 bits)
	struct WINDOWSMIDISERVICES_API Midi2NoteOffMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeNoteOff;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		// we have an enum for attribute type, but other attribute types may 
		// be set in the MIDI CI profile, so we don't restrict
		// See protocol spec 4.2.13 and 4.2.14 for attributes and pitch
		const MidiByte8 getAttributeType();
		void setAttributeType(const MidiByte8 value);

		const MidiShort16 getVelocity();
		void setVelocity(const MidiShort16 value);

		const MidiShort16 getAttributeData();
		void setAttributeData(const MidiShort16 value);

		//Conversion method. Uses MIDI 1.0 rules and data
		static Midi2NoteOffMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 noteNumberByte,
			const MidiByte7 velocityByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2NoteOffMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const Midi2NoteOnOffAttributeType attributeType,
			const MidiShort16 velocity,
			const MidiShort16 attributeData);
	};

	// Protocol spec 4.2.2. MIDI 2.0 note on message. See notes on MIDI 2.0 note attributes
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Attribute Type (8 bits)
	// Short[2] : Velocity (16 bits)
	// Short[3] : Attribute Data (16 bits)
	struct WINDOWSMIDISERVICES_API Midi2NoteOnMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeNoteOn;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		// we have an enum for attribute type, but other attribute types may 
		// be set in the MIDI CI profile, so we don't restrict
		// See protocol spec 4.2.13 and 4.2.14 for attributes and pitch
		const MidiByte8 getAttributeType();
		void setAttributeType(const MidiByte8 value);


		const MidiShort16 getVelocity();
		void setVelocity(const MidiShort16 value);

		const MidiShort16 getAttributeData();
		void setAttributeData(const MidiShort16 value);

		// Conversion method. Uses MIDI 1.0 rules and data
		static Midi2NoteOnMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte8 statusByte,
			const MidiByte7 noteNumberByte,
			const MidiByte7 velocityByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2NoteOnMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const Midi2NoteOnOffAttributeType attributeType,
			const MidiShort16 velocity,
			const MidiShort16 attributeData);
	};

	// Protocol spec 4.2.3. MIDI 2.0 polyphonic pressure (aftertouch) message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PolyPressureMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodePolyPressure;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);


		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2PolyPressureMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.4. MIDI 2.0 registered per-note controller message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Index (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RegisteredPerNoteControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeRegisteredPerNoteController;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiByte8 getIndex();
		void setIndex(const MidiByte8 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2RegisteredPerNoteControllerMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiByte8 index,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.4. MIDI 2.0 assignable per-note controller message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Index (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2AssignablePerNoteControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeAssignablePerNoteController;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiByte8 getIndex();
		void setIndex(const MidiByte8 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);


		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2AssignablePerNoteControllerMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiByte8 index,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.5. MIDI 2.0 per-note management message.
	// Byte[2] : Note Number (7 bits)
	// Byte[3] : Option flags (8 bits)
	// Word[2] : (unused. Set to zero) (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PerNoteManagementMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodePerNoteManagement;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiByte8 getOptionFlags();
		void setOptionFlags(const MidiByte8 value);


		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2PerNoteManagementMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiByte8 optionFlags);
	};

	// Protocol spec 4.2.6. MIDI 2.0 control change message.
	// Byte[2] : Index (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data(32 bits)
	struct WINDOWSMIDISERVICES_API Midi2ControlChangeMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeControlChange;

		const MidiByte7 getIndex();
		void setIndex(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2ControlChangeMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 index,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.7. MIDI 2.0 registered (RPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RegisteredControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const MidiNibble4 Opcode = Midi2ChannelVoiceOpcodeRegisteredController;

		const MidiByte7 getBank();
		void setBank(const MidiByte7 value);

		const MidiByte7 getIndex();
		void setIndex(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2RegisteredControllerMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 bank,
			const MidiByte7 index,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.7. MIDI 2.0 assignable (NRPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2AssignableControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeAssignableController;

		const MidiByte7 getBank();
		void setBank(const MidiByte7 value);

		const MidiByte7 getIndex();
		void setIndex(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2AssignableControllerMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 bank,
			const MidiByte7 index,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.8. MIDI 2.0 relative registered (R-RPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RelativeRegisteredControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeRelativeRegisteredController;

		const MidiByte7 getBank();
		void setBank(const MidiByte7 value);

		const MidiByte7 getIndex();
		void setIndex(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2RelativeRegisteredControllerMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 bank,
			const MidiByte7 index,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.8. MIDI 2.0 relative assignable (R-NRPN) controller message.
	// Byte[2] : Bank (7 bits)
	// Byte[3] : Index (7 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2RelativeAssignableControllerMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeRelativeAssignableController;

		const MidiByte7 getBank();
		void setBank(const MidiByte7 value);

		const MidiByte7 getIndex();
		void setIndex(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2RelativeAssignableControllerMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 bank,
			const MidiByte7 index,
			const MidiWord32 data);
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
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeProgramChange;

		const Midi2ProgramChangeOptionFlags getOptionFlags();
		void setOptionFlags(const Midi2ProgramChangeOptionFlags value);

		const MidiByte7 getProgram();
		void setProgram(const MidiByte7 value);

		const MidiByte7 getBankMsb();
		void setBankMsb(const MidiByte7 value);

		const MidiByte7 getBankLsb();
		void setBankLsb(const MidiByte7 value);


		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2ProgramChangeMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const Midi2ProgramChangeOptionFlags optionFlags,
			const MidiByte7 program,
			const MidiByte7 bankMsb,
			const MidiByte7 bankLsb);
	};

	// Protocol spec 4.2.10. MIDI 2.0 channel pressure (aftertouch) message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2ChannelPressureMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodeChannelPressure;

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2ChannelPressureMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.11. MIDI 2.0 channel pitch bend message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PitchBendMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodePitchBend;

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2PitchBendMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiWord32 data);
	};

	// Protocol spec 4.2.12. MIDI 2.0 per-note pitch bend message.
	// Byte[2] : Note number (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	// Word[2] : Data (32 bits)
	struct WINDOWSMIDISERVICES_API Midi2PerNotePitchBendMessage final : public Midi2ChannelVoiceMessage
	{
		const Midi2ChannelVoiceOpcode Opcode = Midi2ChannelVoiceOpcodePerNotePitchBend;

		const MidiByte7 getNoteNumber();
		void setNoteNumber(const MidiByte7 value);

		const MidiWord32 getData();
		void setData(const MidiWord32 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2PerNotePitchBendMessage FromValues(
			const MidiGroup group,
			const MidiChannel channel,
			const MidiByte7 noteNumber,
			const MidiWord32 data);
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : Data
	// ----------------------------------------------------------------------------


	// Protocol spec 4.4. MIDI 7-bit MIDI 1.0-data-compatible System Exclusive message
	// Byte[1]   : High Nibble: Status, Low Nibble # bytes (8 bits total)
	// Byte[2-7] : Data (7 bits each)
	// See notes in spec as this is not identical to a MIDI 1.0 sysex stream
	struct WINDOWSMIDISERVICES_API MidiSystemExclusive7Message final : public Ump64
	{
		const MidiMessageType MessageType = MidiMessageTypeSystemExclusive7Bit;

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);
	
		const MidiSystemExclusive7MessageStatus getStatus();
		void setStatus(const MidiSystemExclusive7MessageStatus value);

		const MidiNibble4 getNumDataBytes();
		void setNumDataBytes(const MidiNibble4 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static MidiSystemExclusive7Message FromValues(
			const MidiGroup group,
			const MidiSystemExclusive7MessageStatus status,
			const MidiNibble4 numDataBytes,
			const byte bytes[]);
	};



	// Protocol spec 4.5. MIDI 2 8-bit system exclusive message.
	// Recommended for new implementations which don't require 7-bit MIDI 1.0 
	// compatibility. Using this message can cut down on total bytes transferred 
	// by using all 8 bits of each data byte.
	// Byte[1]	  : High Nibble: Status, Low Nibble # data bytes (8 bits total)
	// Byte[2]    : Stream Id
	// Byte[3-15] : Data (8 bits each)
	struct WINDOWSMIDISERVICES_API Midi2SystemExclusive8BitMessage final : public Ump128
	{
		const MidiMessageType MessageType = MidiMessageTypeMidi2Data;

		const MidiSystemExclusive8MessageStatus getStatus();
		void setStatus(const MidiSystemExclusive8MessageStatus value);

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

		const uint8_t getNumBytes();
		void setNumBytes(const uint8_t value);

		const uint8_t getStreamId();
		void setStreamId(const uint8_t value);

		// TODO data get/set methods

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static MidiSystemExclusive7Message FromValues(
			const MidiGroup group,
			const MidiSystemExclusive8MessageStatus status,
			const MidiNibble4 numDataBytes,
			const MidiByte8 streamId,
			const byte bytes[]);
	};



	// Protocol spec 4.6. MIDI 2.0 mixed data set header message. 
	// Recommended for new implementations which don't require 7-bit MIDI 1.0 
	// compatibility. Using this message can cut down on total bytes transferred 
	// by using all 8 bits of each data byte.
	// Byte[1]	  : High Nibble: Status, Low Nibble MDS Id (8 bits total)
	// Short[1]   : Number of valid bytes in this chunk (including header)
	// Short[2]   : Number of chunks in mixed data set
	// Short[3]   : Number of this chunk (1-based)
	// Short[4]   : Manufacturer Id
	// Short[5]   : Device Id
	// Short[6]   : Sub Id 1
	// Short[7]   : Sub Id 2
	struct WINDOWSMIDISERVICES_API Midi2MixedDataSetHeaderMessage final : public Ump128
	{
		const MidiMessageType MessageType = MidiMessageTypeMidi2Data;
		const Midi2MixedMessageStatus Status = Midi2MixedDataStatusHeader;

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

		const MidiNibble4 getMdsId();
		void setMdsId(const MidiNibble4 value);

		const MidiShort16 getNumberOfValidBytesInChunk();
		void setNumberOfValidBytesInChunk(const MidiShort16 value);

		const MidiShort16 getNumberOfChunks();
		void setNumberOfChunks(const MidiShort16 value);

		const MidiShort16 getChunkNumber();
		void setChunkNumber(const MidiShort16 value);

		const MidiShort16 getManufacturerId();
		void setManufacturerId(const MidiShort16 value);

		const MidiShort16 getDeviceId();
		void setDeviceId(const MidiShort16 value);

		const MidiShort16 getSubId1();
		void setSubId1(const MidiShort16 value);

		const MidiShort16 getSubId2();
		void setSubId2(const MidiShort16 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2MixedDataSetHeaderMessage FromValues(
			const MidiGroup group,
			const MidiNibble4 mdsId,
			const MidiShort16 numberOfValidBytesInChunk,
			const MidiShort16 numberOfChunks,
			const MidiShort16 chunkNumber,
			const MidiShort16 manufacturerId,
			const MidiShort16 deviceId,
			const MidiShort16 subId1,
			const MidiShort16 subId2);

	};

	// Protocol spec 4.6. MIDI 2.0 mixed data set payload message. 
	// Recommended for new implementations which don't require 7-bit MIDI 1.0 
	// compatibility. Using this message can cut down on total bytes transferred 
	// by using all 8 bits of each data byte.
	// Byte[1]	  : High Nibble: Status, Low Nibble MDS Id (8 bits total)
	// Byte[2-15] : Data (can be bytes, shorts, etc.)
	struct WINDOWSMIDISERVICES_API Midi2MixedDataSetPayloadMessage final : public Ump128
	{
		const MidiMessageType MessageType = MidiMessageTypeMidi2Data;
		const Midi2MixedMessageStatus Status = Midi2MixedDataStatusPayload;

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

		const MidiNibble4 getMdsId();
		void setMdsId(const MidiNibble4 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2MixedDataSetPayloadMessage FromValues(
			const MidiGroup group,
			const MidiNibble4 mdsId,
			const MidiByte8 array14bytes[]);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2MixedDataSetPayloadMessage FromValues(
			const MidiGroup group,
			const MidiNibble4 mdsId,
			const MidiShort16 array7Shorts[]);

		// TODO data get/set methods
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : System
	// ----------------------------------------------------------------------------


	struct WINDOWSMIDISERVICES_API MidiSystemMessage : public Ump32
	{
		const MidiMessageType MessageType = MidiMessageTypeSystem;

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

	};

	// Protocol spec 4.3. System MIDI Time Code quarter frame message.
	// Byte[2] : High 3 bits message type, low 4 bits value (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiTimeCodeMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xF1;

		const MidiNibble3 getTimeCodeMessageType();
		void setTimeCodeMessageType(const MidiNibble3 value);

		const MidiNibble4 getTimeCodeValue();
		void setTimeCodeValue(const MidiNibble4 value);


		// Conversion method. Uses MIDI 1.0 rules and data
		static MidiTimeCodeMessage FromMidi1Bytes(
			const MidiGroup group,
			const MidiByte7 timecodeByte);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static MidiTimeCodeMessage FromValues(
			const MidiGroup group,
			const MidiNibble3 timeCodeMessageType,
			const MidiNibble4 timeCodeValue);
	};

	// Protocol spec 4.3. System MIDI Song Position Pointer message.
	// Byte[2] : Data LSB (7 bits) note: lsb is before msb per MIDI 1.0
	// Byte[3] : Data MSB (7 bits)
	struct WINDOWSMIDISERVICES_API MidiSongPositionPointerMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xF2;

		const MidiByte7 getDataLsb();
		void setDataLsb(const MidiByte7 value);

		const MidiByte7 getDataMsb();
		void setDataMsb(const uint8_t value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static MidiSongPositionPointerMessage FromValues(
			const MidiGroup group,
			const MidiByte7 positionLsbByte,
			const MidiByte7 positionMsbByte);
	};

	// Protocol spec 4.3. System MIDI Song Select message.
	// Byte[2] : Song (7 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiSongSelectMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xF3;

		const MidiByte7 getSong();
		void setSong(const MidiByte7 value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static MidiSongSelectMessage FromValues(
			const MidiGroup group,
			const MidiByte7 songByte);
	};

	// Protocol spec 4.3. System MIDI Tune Request message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiTuneRequestMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xF6;

	// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
	static MidiTuneRequestMessage FromValues(const MidiGroup group);
	};

	// Protocol spec 4.3. System MIDI Timing Clock message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiTimingClockMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xF8;

	// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
	static MidiTimingClockMessage FromValues(const MidiGroup group);
	};

	// Protocol spec 4.3. System MIDI Start message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiStartMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xFA;

	// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
	static MidiStartMessage FromValues(const MidiGroup group);
	};

	// Protocol spec 4.3. System MIDI Continue message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiContinueMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xFB;

	// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
	static MidiContinueMessage FromValues(const MidiGroup group);
	};

	// Protocol spec 4.3. System MIDI Stop message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiStopMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xFC;

	// Conversion method. Uses MIDI 1.0 rules and data
	static MidiStopMessage FromMidi1Bytes(const MidiGroup group);
	};

	// Protocol spec 4.3. System MIDI Active Sensing message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiActiveSensingMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xFE;

	// Conversion method. Uses MIDI 1.0 rules and data
	static MidiActiveSensingMessage FromMidi1Bytes(const MidiGroup group);
	};

	// Protocol spec 4.3. System MIDI Reset message.
	// Byte[2] : (unused. Set to zero) (8 bits)
	// Byte[3] : (unused. Set to zero) (8 bits)
	struct WINDOWSMIDISERVICES_API MidiResetMessage final : public MidiSystemMessage
	{
		const MidiByte8 Status = 0xFF;

	// Conversion method. Uses MIDI 1.0 rules and data
	static MidiResetMessage FromMidi1Bytes(const MidiGroup group);
	};


	// ----------------------------------------------------------------------------
	// Strongly-typed messages : Utility
	// ----------------------------------------------------------------------------


	// Protocol spec 4.8. Utility messages
	struct WINDOWSMIDISERVICES_API Midi2UtilityMessage : public Ump32
	{
		const MidiMessageType MessageType = MidiMessageTypeUtility;

		const MidiGroup getGroup();
		void setGroup(const MidiGroup value);

	};

	// Protocol spec 4.8.1 Utility - No Operation message
	struct WINDOWSMIDISERVICES_API Midi2NoopMessage final : public Midi2UtilityMessage
	{
		// TODO fill this out
	};

	// Protocol spec 4.8.5 Utility - JR Clock Message
	struct WINDOWSMIDISERVICES_API Midi2JitterReductionClockMessage final : public Midi2UtilityMessage
	{
		const uint8_t Status = 0x01;

		const uint16_t getClockTime();
		void setClockTime(const uint16_t value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2JitterReductionClockMessage FromValues(
			const MidiGroup group,
			const MidiShort16 clockTime);

		// TODO fill this out
	};

	// Protocol spec 4.8.5 Utility - JR Timestamp Message
	struct WINDOWSMIDISERVICES_API Midi2JitterReductionTimestampMessage final : public Midi2UtilityMessage
	{
		const uint8_t Status = 0x02;

		const uint16_t getTimestamp();
		void setTimestamp(const uint16_t value);

		// Factory method. Includes data cleanup. To avoid that, use the direct data members (Byte[], Short[], Word[]).
		static Midi2JitterReductionTimestampMessage FromValues(
			const MidiGroup group,
			const MidiShort16 timestamp);

		// TODO fill this out
	};


	// ----------------------------------------------------------------------------
	// MIDI CI
	// ----------------------------------------------------------------------------




	// TODO *****************************



}