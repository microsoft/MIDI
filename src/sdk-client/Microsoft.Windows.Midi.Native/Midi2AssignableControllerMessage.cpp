// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2AssignableControllerMessage::getBank()
	{
		return 0;
	}

	void Midi2AssignableControllerMessage::setBank(const uint8_t value)
	{
	}

	const uint8_t Midi2AssignableControllerMessage::getIndex()
	{
		return 0;
	}

	void Midi2AssignableControllerMessage::setIndex(const uint8_t value)
	{
	}

	const uint32_t Midi2AssignableControllerMessage::getData()
	{
		return 0;
	}

	void Midi2AssignableControllerMessage::setData(const uint32_t value)
	{
	}

	Midi2AssignableControllerMessage Midi2AssignableControllerMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t bank, const uint8_t index, const uint32_t data)
	{
		Midi2AssignableControllerMessage msg;
		return msg;

	}



}