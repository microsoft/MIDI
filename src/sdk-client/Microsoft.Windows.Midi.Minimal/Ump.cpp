
// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Native
{

	// Ump32 --------------------------------------------------------------------------------------------------

	virtual const uint8_t Ump32::getMessageType(const uint8_t value)
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	virtual const uint8_t Ump32::getGroup(const uint8_t value)
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	virtual void Ump32::setGroup(const uint8_t value)
	{
		SetLeastSignificantNibble(&Byte[0], value);
	}

	// Ump64 --------------------------------------------------------------------------------------------------

	virtual const uint8_t Ump64::getMessageType(const uint8_t value)
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	virtual const uint8_t Ump64::getGroup(const uint8_t value)
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	virtual void Ump64::setGroup(const uint8_t value)
	{
		SetLeastSignificantNibble(&Byte[0], value);
	}

	// Ump96 --------------------------------------------------------------------------------------------------

	virtual const uint8_t Ump96::getMessageType(const uint8_t value)
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	virtual const uint8_t Ump96::getGroup(const uint8_t value)
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	virtual void Ump96::setGroup(const uint8_t value)
	{
		SetLeastSignificantNibble(&Byte[0], value);
	}


	// Ump128 --------------------------------------------------------------------------------------------------

	virtual const uint8_t Ump128::getMessageType(const uint8_t value)
	{
		return Utility::MostSignificantNibble(Byte[0]);
	}

	virtual const uint8_t Ump128::getGroup(const uint8_t value)
	{
		return Utility::LeastSignificantNibble(Byte[0]);
	}

	virtual void Ump128::setGroup(const uint8_t value)
	{
		SetLeastSignificantNibble(&Byte[0], value);
	}

}
