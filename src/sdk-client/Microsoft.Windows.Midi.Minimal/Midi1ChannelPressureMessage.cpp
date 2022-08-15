#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Native
{

	const uint8_t Midi1ChannelPressureMessage::getData()
	{
	}

	void Midi1ChannelPressureMessage::setData(const uint8_t value)
	{
	}

	static Midi1ChannelPressureMessage Midi1ChannelPressureMessage::FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t dataByte);
	{
	}

	static Midi1ChannelPressureMessage Midi1ChannelPressureMessage::FromValues(const uint8_t group, const uint8_t channel, const uint8_t data)
	{
	}



}