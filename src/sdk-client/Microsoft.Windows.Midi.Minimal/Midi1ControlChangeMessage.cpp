#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Native
{

	const uint8_t Midi1ControlChangeMessage::getData()
	{
	}

	void Midi1ControlChangeMessage::setData(const uint8_t value)
	{
	}



	// Protocol spec 4.1.4. MIDI 1.0 control change message
	struct WINDOWSMIDISERVICES_API Midi1ControlChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xB;

		const uint8_t getIndex();
		void setIndex(const uint8_t value);

		const uint8_t getData();
		void setData(const uint8_t value);

		static Midi1ControlChangeMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t indexByte, const uint8_t dataByte);
		static Midi1ControlChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t index, const uint8_t data);
	};
}