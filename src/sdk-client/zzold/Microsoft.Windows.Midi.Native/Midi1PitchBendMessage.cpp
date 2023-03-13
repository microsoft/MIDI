// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi1PitchBendMessage::getDataLsb()
	{
		return 0;
	}

	void Midi1PitchBendMessage::setDataLsb(const uint8_t value)
	{
	}

	const uint8_t Midi1PitchBendMessage::getDataMsb()
	{
		return 0;
	}

	void Midi1PitchBendMessage::setDataMsb(const uint8_t value)
	{
	}

	const uint16_t Midi1PitchBendMessage::getDataCombined()
	{
		return 0;
	}

	void Midi1PitchBendMessage::setDataCombined(const uint16_t value)
	{
	}

	Midi1PitchBendMessage Midi1PitchBendMessage::FromMidi1Bytes(const MidiGroup group, const uint8_t statusByte, const uint8_t lsbDataByte, const uint8_t msbDataByte)
	{
		Midi1PitchBendMessage msg;
		return msg;

	}

	Midi1PitchBendMessage Midi1PitchBendMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t dataLsb, const uint8_t dataMsb)
	{
		Midi1PitchBendMessage msg;
		return msg;
	}

	Midi1PitchBendMessage Midi1PitchBendMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint16_t data)
	{
		Midi1PitchBendMessage msg;
		return msg;
	}

}