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

#include <stdint.h>

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

namespace Microsoft::Windows::Midi
{

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
			uint32_t Word[1] = {};
			uint16_t Short[2];
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
			uint32_t Word[2] = {};
			uint16_t Short[4];
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
			uint32_t Word[3] = {};
			uint16_t Short[6];
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
			uint32_t Word[4] = {};
			uint16_t Short[8];
			uint8_t Byte[16];
		};

		virtual const uint8_t getMessageType();
		virtual const uint8_t getGroup();
		virtual void setGroup(const uint8_t value);
	};


}