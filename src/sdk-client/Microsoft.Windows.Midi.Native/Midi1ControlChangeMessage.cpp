// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi1ControlChangeMessage::getData()
	{
		return 0;
	}

	void Midi1ControlChangeMessage::setData(const uint8_t value)
	{
	}

	const uint8_t Midi1ControlChangeMessage::getIndex()
	{
		return 0;
	}

	void Midi1ControlChangeMessage::setIndex(const uint8_t value)
	{
	}


	Midi1ControlChangeMessage Midi1ControlChangeMessage::FromMidi1Bytes(const MidiGroup group, const uint8_t statusByte, const uint8_t indexByte, const uint8_t dataByte)
	{
		Midi1ControlChangeMessage msg;
		return msg;
	}

	Midi1ControlChangeMessage Midi1ControlChangeMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t index, const uint8_t data)
	{
		Midi1ControlChangeMessage msg;
		return msg;
	}

}