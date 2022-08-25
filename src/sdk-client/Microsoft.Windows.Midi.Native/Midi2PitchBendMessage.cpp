// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesMessages.h"

namespace Microsoft::Windows::Midi::Messages
{
	const uint32_t Midi2PitchBendMessage::getData()
	{
		return 0;
	}

	void Midi2PitchBendMessage::setData(const uint32_t value)
	{
	}

	Midi2PitchBendMessage Midi2PitchBendMessage::FromValues(const MidiGroup group, const MidiChannel channel, const uint32_t data)
	{
		Midi2PitchBendMessage msg;
		return msg;
	}

}