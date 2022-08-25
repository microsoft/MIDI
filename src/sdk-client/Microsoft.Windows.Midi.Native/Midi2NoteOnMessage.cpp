// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi2NoteOnMessage::getNoteNumber()
	{
		return 0;
	}

	void Midi2NoteOnMessage::setNoteNumber(const uint8_t value)
	{
	}

	const uint8_t Midi2NoteOnMessage::getAttributeType()
	{
		return 0;
	}

	void Midi2NoteOnMessage::setAttributeType(const uint8_t value)
	{
	}

	const uint16_t Midi2NoteOnMessage::getVelocity()
	{
		return 0;
	}

	void Midi2NoteOnMessage::setVelocity(const uint16_t value)
	{
	}

	const uint16_t Midi2NoteOnMessage::getAttributeData()
	{
		return 0;
	}

	void Midi2NoteOnMessage::setAttributeData(const uint16_t value)
	{
	}



	Midi2NoteOnMessage Midi2NoteOnMessage::FromMidi1Bytes(const MidiGroup group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte)
	{
		Midi2NoteOnMessage msg;
		return msg;
	}

	Midi2NoteOnMessage Midi2NoteOnMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t noteNumber, const Midi2NoteOnOffAttributeType attributeType, const uint16_t velocity, const uint16_t attributeData)
	{
		Midi2NoteOnMessage msg;
		return msg;
	}
}