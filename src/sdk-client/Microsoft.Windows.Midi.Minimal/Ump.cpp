
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
		Byte[0] &= 0xF0;
		Byte[0] |= Utility::LeastSignificantNibble(value);
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
		Byte[0] &= 0xF0;
		Byte[0] |= Utility::LeastSignificantNibble(value);
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
		Byte[0] &= 0xF0;
		Byte[0] |= Utility::LeastSignificantNibble(value);
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
		Byte[0] &= 0xF0;
		Byte[0] |= Utility::LeastSignificantNibble(value);
	}

}
