// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2ControlChangeMessage::getIndex()
	{
		return 0;
	}

	void Midi2ControlChangeMessage::setIndex(const uint8_t value)
	{
	}

	const uint32_t Midi2ControlChangeMessage::getData()
	{
		return 0;
	}

	void Midi2ControlChangeMessage::setData(const uint32_t value)
	{
	}

	Midi2ControlChangeMessage Midi2ControlChangeMessage::FromValues(const uint8_t group, const uint8_t channel, const uint8_t index, const uint32_t data)
	{
		Midi2ControlChangeMessage msg;
		return msg;

	}


}