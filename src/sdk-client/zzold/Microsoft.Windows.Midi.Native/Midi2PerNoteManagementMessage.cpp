// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2PerNoteManagementMessage::getNoteNumber()
	{
		return 0;
	}

	void Midi2PerNoteManagementMessage::setNoteNumber(const uint8_t value)
	{
	}

	const uint8_t Midi2PerNoteManagementMessage::getOptionFlags()
	{
		return 0;
	}

	void Midi2PerNoteManagementMessage::setOptionFlags(const uint8_t value)
	{
	}

	Midi2PerNoteManagementMessage Midi2PerNoteManagementMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t noteNumber, const uint8_t optionFlags)
	{
		Midi2PerNoteManagementMessage msg;
		return msg;
	}



}