// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi1ChannelVoiceMessage::getOpcode()
	{
		return Utility::MostSignificantNibble(Byte[1]);
	}

	const uint8_t Midi1ChannelVoiceMessage::getChannel()
	{
		return Utility::LeastSignificantNibble(Byte[1]);
	}

	void Midi1ChannelVoiceMessage::setChannel(const uint8_t value)
	{
		Utility::SetLeastSignificantNibble(&Byte[1], value);
	}


}