// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

#pragma once

#include <stdint.h>

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

// ----------------------------------------------------------------------------
// Some potentially helpful things
// ----------------------------------------------------------------------------


namespace Microsoft::Windows::Midi::Utility
{
	// Need some unit tests for these on both x64 and Arm64
	constexpr uint8_t Make7BitByte(uint8_t b) { return b & 0x7F; }

	constexpr uint16_t Make16BitManufacturerId(uint8_t b) { return (uint16_t)(b & 0x7F); }	// MSB is all zero, including high bit
	constexpr uint16_t Make16BitManufacturerId(uint8_t b1, uint8_t b2, uint8_t b3) { return (uint16_t)((b3 & 0x7F) | ((b2 & 0x7F) << 8) | 0x8000); }	// byte 1 is ignored, high bit is set high

	constexpr uint16_t Combine14BitsMsbLsb(uint8_t msb, uint8_t lsb) { return (uint16_t)((msb & 0x7F) << 8 | (lsb & 0x7F)); }

	constexpr uint8_t Msb(uint16_t number) { return (uint8_t)(number & 0xFF00 >> 8); }
	constexpr uint8_t Lsb(uint16_t number) { return (uint8_t)(number & 0x00FF); }

	constexpr uint16_t Msint16(uint32_t word) { return (uint16_t)(word & 0xFFFF0000 >> 16); }
	constexpr uint16_t Lsint16(uint32_t word) { return (uint16_t)(word & 0x0000FFFF); }

	constexpr uint8_t MostSignificantNibble(uint8_t b) { return (b & 0xF0) >> 4; }
	constexpr uint8_t LeastSignificantNibble(uint8_t b) { return b & 0x0F; }

	constexpr void SetLeastSignificantNibble(uint8_t& b, uint8_t nibbleValue) { b = ((b & 0xF0) | (nibbleValue & 0x0F)); }
	constexpr void SetMostSignificantNibble(uint8_t& b, uint8_t nibbleValue) { b = ((b & 0x0F) | (nibbleValue << 4)); }


}