// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Messages
{
	static Midi1NoteOffMessage Midi1NoteOffMessage::FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte)
	{
	}

	static Midi1NoteOffMessage Midi1NoteOffMessage::FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t velocity)
	{
	}




	// Protocol spec 4.1.1. MIDI 1.0 note off message
	struct WINDOWSMIDISERVICES_API Midi1NoteOffMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0x8;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getVelocity();
		void setVelocity(const uint8_t value);

	};

}