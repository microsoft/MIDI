#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Native
{



	// Protocol spec 4.1.5. MIDI 1.0 program change message
	struct WINDOWSMIDISERVICES_API Midi1ProgramChangeMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xC;

		const uint8_t getProgram();
		void setProgram(const uint8_t value);

		static Midi1ProgramChangeMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t programByte);
		static Midi1ProgramChangeMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t program);
	};
}