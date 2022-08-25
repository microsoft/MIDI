// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2NoteOffMessage::getNoteNumber()
	{
		return 0;
	}

	void Midi2NoteOffMessage::setNoteNumber(const uint8_t value)
	{
	}

	const uint8_t Midi2NoteOffMessage::getAttributeType()
	{
		return 0;
	}

	void Midi2NoteOffMessage::setAttributeType(const uint8_t value)
	{
	}

	const uint16_t Midi2NoteOffMessage::getVelocity()
	{
		return 0;
	}

	void Midi2NoteOffMessage::setVelocity(const uint16_t value)
	{
	}

	const uint16_t Midi2NoteOffMessage::getAttributeData()
	{
		return 0;
	}

	void Midi2NoteOffMessage::setAttributeData(const uint16_t value)
	{
	}



	Midi2NoteOffMessage Midi2NoteOffMessage::FromMidi1Bytes(const MidiGroup group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte)
	{
		Midi2NoteOffMessage msg;
		return msg;
	}

	Midi2NoteOffMessage Midi2NoteOffMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t noteNumber, const Midi2NoteOnOffAttributeType attributeType, const uint16_t velocity, const uint16_t attributeData)
	{
		Midi2NoteOffMessage msg;
		return msg;
	}
}