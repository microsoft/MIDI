
// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesUmp.h"
#include "WindowsMidiServicesUtility.h"

namespace Microsoft::Windows::Midi::Messages
{

	// Ump32 --------------------------------------------------------------------------------------------------

	const uint8_t Ump32::getMessageType()
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	const uint8_t Ump32::getGroup()
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump32::setGroup(const uint8_t value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

	// Ump64 --------------------------------------------------------------------------------------------------

	const uint8_t Ump64::getMessageType()
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	const uint8_t Ump64::getGroup()
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump64::setGroup(const uint8_t value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

	// Ump96 --------------------------------------------------------------------------------------------------

	const uint8_t Ump96::getMessageType()
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	const uint8_t Ump96::getGroup()
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump96::setGroup(const uint8_t value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}


	// Ump128 --------------------------------------------------------------------------------------------------

	const uint8_t Ump128::getMessageType()
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	const uint8_t Ump128::getGroup()
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	void Ump128::setGroup(const uint8_t value)
	{
		Utility::SetLeastSignificantNibble(Byte[0], value);
	}

}
