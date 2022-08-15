#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Native
{



	// Protocol spec 4.1.7. MIDI 1.0 pitch bend message
	struct WINDOWSMIDISERVICES_API Midi1PitchBendMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xE;

		const uint8_t getDataLsb();
		void setDataLsb(const uint8_t value);

		const uint8_t getDataMsb();
		void setDataMsb(const uint8_t value);

		const uint16_t getDataCombined();
		void setDataCombined(const uint16_t value);

		static Midi1PitchBendMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t lsbDataByte, const uint8_t msbDataByte);
		static Midi1PitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t dataLsb, const uint8_t dataMsb);
		static Midi1PitchBendMessage FromValues(const uint8_t group, const uint8_t channel, const uint16_t data);
	};
}