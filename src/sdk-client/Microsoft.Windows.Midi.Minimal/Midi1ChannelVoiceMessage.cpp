#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Native
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
		Byte[1] |= Utility::LeastSignificantNibble(value);
	}


}