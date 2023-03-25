// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================


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

#pragma once


#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include "WindowsMidiServicesBase.h"

namespace Microsoft::Windows::Midi::Messages //::inline v0_1_0_pre
{
	// cast this to an int to get number of words in the packet
	enum WINDOWSMIDISERVICES_API UmpType
	{
		UmpTypeInvalidOrUnknown = 0,
		UmpType32 = 1,
		UmpType64 = 2,
		UmpType96 = 3,
		UmpType128 = 4
	};


	// Base message structure
	struct WINDOWSMIDISERVICES_API Ump
	{
		virtual const MidiMessageType getMessageType() = 0;

		// *** TODO: Group field is no longer included in everything (type 0, type F) 
		// So moved group out of thse types and into the strongly typed classes. Need to verify they are correct ***		

		virtual bool setFromWords(MidiWord32* wordBuffer, bool validateFirst) = 0;

		static UmpType GetTypeFromFirstWord(MidiWord32 word);
	};

	// 32 bit (1 word) MIDI message. Used for all MIDI 1.0 messages, utility messages, and more
	struct WINDOWSMIDISERVICES_API Ump32 : virtual public Ump
	{
		union
		{
			MidiWord32		Word[1] = {};
			MidiShort16		Short[2];
			MidiByte8		Byte[4];
		};

		virtual const MidiMessageType getMessageType();


		virtual bool setFromWords(MidiWord32* wordBuffer, bool validateFirst);
	};

	// 64 bit (2 words) MIDI message. Used for MIDI 2.0 channel voice, SysEx7, and more
	struct WINDOWSMIDISERVICES_API Ump64 : virtual public Ump
	{
		union
		{
			MidiWord32		Word[2] = {};
			MidiShort16		Short[4];
			MidiByte8		Byte[8];
		};

		virtual const MidiMessageType getMessageType();

		virtual bool setFromWords(MidiWord32* wordBuffer, bool validateFirst);
	};

	// 96 bit (3 words) MIDI message. Not currently used for any types of MIDI messages
	struct WINDOWSMIDISERVICES_API Ump96 : virtual public Ump
	{
		union
		{
			MidiWord32		Word[3] = {};
			MidiShort16		Short[6];
			MidiByte8		Byte[12];
		};

		virtual const MidiMessageType getMessageType();

		virtual bool setFromWords(MidiWord32* wordBuffer, bool validateFirst);
	};

	// 128 bit (4 words) MIDI message. Used for mixed data and 8-bit SysEx messages
	struct WINDOWSMIDISERVICES_API Ump128 : virtual public Ump
	{
		union
		{
			MidiWord32		Word[4] = {};
			MidiShort16		Short[8];
			MidiByte8		Byte[16];
		};

		virtual const MidiMessageType getMessageType();

		virtual bool setFromWords(MidiWord32* wordBuffer, bool validateFirst);
	};

}