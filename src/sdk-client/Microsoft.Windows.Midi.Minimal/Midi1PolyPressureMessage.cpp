// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Messages
{

	const uint8_t Midi1PolyPressureMessage::getData()
	{
	}

	void Midi1PolyPressureMessage::setData(const uint8_t value)
	{
	}

	// Protocol spec 4.1.3. MIDI 1.0 polyphonic pressure (aftertouch) message
	struct WINDOWSMIDISERVICES_API Midi1PolyPressureMessage final : public Midi1ChannelVoiceMessage
	{
		const uint8_t Opcode = 0xA;

		const uint8_t getNoteNumber();
		void setNoteNumber(const uint8_t value);

		const uint8_t getData();
		void setData(const uint8_t value);

		static Midi1PolyPressureMessage FromMidi1Bytes(const uint8_t group, const uint8_t statusByte, const uint8_t noteNumberByte, const uint8_t dataByte);
		static Midi1PolyPressureMessage FromValues(const uint8_t group, const uint8_t channel, const uint8_t noteNumber, const uint8_t data);
	};
}