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
	}

	void Midi1NoteOffMessage::setNoteNumber(const uint8_t value)
	{
	}

	const uint8_t Midi1NoteOffMessage::getVelocity()
	{
	}

	void Midi1NoteOffMessage::setVelocity(const uint8_t value)
	{
	}


	Midi1NoteOffMessage Midi1NoteOffMessage::FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t velocityByte)
	{
	}

	Midi1NoteOffMessage Midi1NoteOffMessage::FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t velocity)
	{
	}

}