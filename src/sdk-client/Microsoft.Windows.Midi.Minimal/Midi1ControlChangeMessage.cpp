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
	}

	void Midi1ControlChangeMessage::setData(const uint8_t value)
	{
	}

	const uint8_t Midi1ControlChangeMessage::getIndex()
	{
	}

	void Midi1ControlChangeMessage::setIndex(const uint8_t value)
	{
	}


	Midi1ControlChangeMessage Midi1ControlChangeMessage::FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t indexByte, const uint8_t dataByte)
	{
	}

	Midi1ControlChangeMessage Midi1ControlChangeMessage::FromValues(const uint8_t group, const uint8_t channel, const uint8_t index, const uint8_t data)
	{
	}

}