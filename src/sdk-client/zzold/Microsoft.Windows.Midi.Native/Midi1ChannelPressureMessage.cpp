// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi1ChannelPressureMessage::getData()
	{
		return 0;
	}

	void Midi1ChannelPressureMessage::setData(const uint8_t value)
	{
	}

	Midi1ChannelPressureMessage Midi1ChannelPressureMessage::FromMidi1Bytes(const MidiGroup group, const uint8_t statusByte, const uint8_t dataByte)
	{
		Midi1ChannelPressureMessage msg {};
		return msg;

	}

	Midi1ChannelPressureMessage Midi1ChannelPressureMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint8_t data)
	{
		Midi1ChannelPressureMessage msg {};
		return msg;
	}



}