// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint8_t Midi1ProgramChangeMessage::getProgram()
	{
		return 0;
	}

	void Midi1ProgramChangeMessage::setProgram(const uint8_t value)
	{
	}

	Midi1ProgramChangeMessage Midi1ProgramChangeMessage::FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t programByte)
	{
		Midi1ProgramChangeMessage msg;
		return msg;

	}

	Midi1ProgramChangeMessage Midi1ProgramChangeMessage::FromValues(const uint8_t group, const uint8_t channel, const uint8_t program)
	{
		Midi1ProgramChangeMessage msg;
		return msg;
	}
}