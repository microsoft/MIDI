
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

}
