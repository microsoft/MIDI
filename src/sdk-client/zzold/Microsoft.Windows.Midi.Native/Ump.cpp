
// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesUmp.h"
#include "WindowsMidiServicesUtility.h"

#include <memory>


namespace Microsoft::Windows::Midi::Messages
{

	 
	UmpType Ump::GetTypeFromFirstWord(MidiWord32 word)
	{
		// TODO: Read first nibble and return the appropriate UmpType
		int messageType = Utility::MostSignificantNibble(word);


		switch ((int)messageType)
		{
		case (int)MidiMessageType::MidiMessageTypeUtility:				// 0x0: Utility message
		case (int)MidiMessageType::MidiMessageTypeSystem:				// 0x1: system real time and system common
		case (int)MidiMessageType::MidiMessageTypeMidi1ChannelVoice:		// 0x2: MIDI 1.0 channel voice
			return UmpType::UmpType32;

		case (int)MidiMessageType::MidiMessageTypeSystemExclusive7Bit:	// 0x3: Midi 1 data message
		case (int)MidiMessageType::MidiMessageTypeMidi2ChannelVoice:		// 0x4: MIDI 2.0 channel voice
			return UmpType::UmpType64;

		case (int)MidiMessageType::MidiMessageTypeMidi2Data:				// 0x5: Midi 2 Data
			return UmpType::UmpType128;

			// future reserved message types --------

		case 0x6:   // reserved
		case 0x7:   // reserved
			return UmpType::UmpType32;
		case 0x8:   // reserved
		case 0x9:   // reserved
		case 0xA:   // reserved
			return UmpType::UmpType64;
		case 0xB:   // reserved
		case 0xC:   // reserved
			return UmpType::UmpType96;
		case 0xD:   // reserved
		case 0xE:   // reserved
		case 0xF:   // reserved
			return UmpType::UmpType128;
		
		default:
			// this can't actually happen, but it's here
			return UmpType::UmpTypeInvalidOrUnknown;
		}
	}
	 
	
	// Ump32 --------------------------------------------------------------------------------------------------

	const MidiMessageType Ump32::getMessageType()
	{
		return (MidiMessageType)Utility::MostSignificantNibble(Byte[0]);
	}

	const MidiGroup Ump32::getGroup()
	{
		return (MidiGroup)Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump32::setGroup(const MidiGroup value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

	bool Ump32::setFromWords(MidiWord32* wordBuffer, bool validateFirst)
	{
		// if validateFirst, get word count from first nibble and ensure it matches the Ump32 type
		// TODO
	}

	// Ump64 --------------------------------------------------------------------------------------------------

	const MidiMessageType Ump64::getMessageType()
	{
		return (MidiMessageType)Utility::MostSignificantNibble(Byte[0]);
	}

	const MidiGroup Ump64::getGroup()
	{
		return (MidiGroup)Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump64::setGroup(const MidiGroup value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

	bool Ump64::setFromWords(MidiWord32* wordBuffer, bool validateFirst)
	{
		// if validateFirst, get word count from first nibble and ensure it matches the Ump64 type
		// TODO
	}


	// Ump96 --------------------------------------------------------------------------------------------------

	const MidiMessageType Ump96::getMessageType()
	{
		return (MidiMessageType)Utility::MostSignificantNibble(Byte[0]);
	}

	const MidiGroup Ump96::getGroup()
	{
		return (MidiGroup)Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump96::setGroup(const MidiGroup value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

	bool Ump96::setFromWords(MidiWord32* wordBuffer, bool validateFirst)
	{
		// if validateFirst, get word count from first nibble and ensure it matches the Ump96 type
		// TODO
	}


	// Ump128 --------------------------------------------------------------------------------------------------

	const MidiMessageType Ump128::getMessageType()
	{
		return (MidiMessageType)Utility::MostSignificantNibble(Byte[0]);
	}

	const MidiGroup Ump128::getGroup()
	{
		return (MidiGroup)Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump128::setGroup(const MidiGroup value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

	bool Ump128::setFromWords(MidiWord32* wordBuffer, bool validateFirst)
	{
		// if validateFirst, get word count from first nibble and ensure it matches the Ump128 type
		// TODO
	}



}
