// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2RegisteredControllerMessage::getBank()
	{
		return 0;
	}

	void Midi2RegisteredControllerMessage::setBank(const uint8_t value)
	{
	}

	const uint8_t Midi2RegisteredControllerMessage::getIndex()
	{
		return 0;
	}

	void Midi2RegisteredControllerMessage::setIndex(const uint8_t value)
	{
	}

	const uint32_t Midi2RegisteredControllerMessage::getData()
	{
		return 0;
	}

	void Midi2RegisteredControllerMessage::setData(const uint32_t value)
	{
	}

	Midi2RegisteredControllerMessage Midi2RegisteredControllerMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t bank, const uint8_t index, const uint32_t data)
	{
		Midi2RegisteredControllerMessage msg;
		return msg;
	}


}