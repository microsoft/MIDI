// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi2RelativeRegisteredControllerMessage::getBank()
	{
		return 0;
	}

	void Midi2RelativeRegisteredControllerMessage::setBank(const uint8_t value)
	{
	}

	const uint8_t Midi2RelativeRegisteredControllerMessage::getIndex()
	{
		return 0;
	}

	void Midi2RelativeRegisteredControllerMessage::setIndex(const uint8_t value)
	{
	}

	const uint32_t Midi2RelativeRegisteredControllerMessage::getData()
	{
		return 0;
	}

	void Midi2RelativeRegisteredControllerMessage::setData(const uint32_t value)
	{
	}

	Midi2RelativeRegisteredControllerMessage Midi2RelativeRegisteredControllerMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t bank, const uint8_t index, const uint32_t data)
	{
		Midi2RelativeRegisteredControllerMessage msg;
		return msg;
	}



}