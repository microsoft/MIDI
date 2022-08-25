// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi1NoteOffMessage::getNoteNumber()
	{
		return 0;
	}

	void Midi1NoteOffMessage::setNoteNumber(const uint8_t value)
	{
	}

	const uint8_t Midi1NoteOffMessage::getVelocity()
	{
		return 0;
	}

	void Midi1NoteOffMessage::setVelocity(const uint8_t value)
	{
	}


	Midi1NoteOffMessage Midi1NoteOffMessage::FromMidi1Bytes(const MidiGroup group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte)
	{
		Midi1NoteOffMessage msg;
		return msg;

	}

	Midi1NoteOffMessage Midi1NoteOffMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t noteNumber, const uint8_t velocity)
	{
		Midi1NoteOffMessage msg;
		return msg;
	}

}