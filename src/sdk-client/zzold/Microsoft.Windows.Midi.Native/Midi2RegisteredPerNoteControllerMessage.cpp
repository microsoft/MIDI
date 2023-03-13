// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2RegisteredPerNoteControllerMessage::getNoteNumber()
	{
		return 0;
	}

	void Midi2RegisteredPerNoteControllerMessage::setNoteNumber(const uint8_t value)
	{
	}

	const uint8_t Midi2RegisteredPerNoteControllerMessage::getIndex()
	{
		return 0;
	}

	void Midi2RegisteredPerNoteControllerMessage::setIndex(const uint8_t value)
	{
	}

	const uint32_t Midi2RegisteredPerNoteControllerMessage::getData()
	{
		return 0;
	}

	void Midi2RegisteredPerNoteControllerMessage::setData(const uint32_t value)
	{
	}

	Midi2RegisteredPerNoteControllerMessage Midi2RegisteredPerNoteControllerMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t noteNumber, const uint8_t index, const uint32_t data)
	{
		Midi2RegisteredPerNoteControllerMessage msg;
		return msg;
	}


}