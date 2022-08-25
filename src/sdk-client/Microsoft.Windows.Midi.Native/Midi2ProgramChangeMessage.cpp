// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const Midi2ProgramChangeOptionFlags Midi2ProgramChangeMessage::getOptionFlags()
	{
		return (Midi2ProgramChangeOptionFlags)0;
	}

	void Midi2ProgramChangeMessage::setOptionFlags(const Midi2ProgramChangeOptionFlags value)
	{
	}

	const uint8_t Midi2ProgramChangeMessage::getProgram()
	{
		return 0;
	}

	void Midi2ProgramChangeMessage::setProgram(const uint8_t value)
	{
	}

	const uint8_t Midi2ProgramChangeMessage::getBankMsb()
	{
		return 0;
	}

	void Midi2ProgramChangeMessage::setBankMsb(const uint8_t value)
	{
	}

	const uint8_t Midi2ProgramChangeMessage::getBankLsb()
	{
		return 0;
	}

	void Midi2ProgramChangeMessage::setBankLsb(const uint8_t value)
	{
	}

	Midi2ProgramChangeMessage Midi2ProgramChangeMessage::FromValues(const MidiGroup group, const MidiChannel channel, const Midi2ProgramChangeOptionFlags optionFlags, const uint8_t program, const uint8_t bankMsb, const uint8_t bankLsb)
	{
		Midi2ProgramChangeMessage msg;
		return msg;
	}


}