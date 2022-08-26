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

#include "WindowsMidiServicesBase.h"

// ----------------------------------------------------------------------------
// Some potentially helpful things for processing when working outside of
// the UMP structure, and its union.
// ----------------------------------------------------------------------------


namespace Microsoft::Windows::Midi::Utility //::inline v0_1_0_pre
{
	// Need some unit tests for these on both x64 and Arm64
	constexpr MidiByte7 Make7BitByte(uint8_t b) { return b & 0x7F; }

	constexpr MidiShort16 Make16BitManufacturerId(uint8_t b) { return (uint16_t)(b & 0x7F); }	// MSB is all zero, including high bit
	constexpr MidiShort16 Make16BitManufacturerId(uint8_t b1, uint8_t b2, uint8_t b3) { return (uint16_t)((b3 & 0x7F) | ((b2 & 0x7F) << 8) | 0x8000); }	// byte 1 is ignored, high bit is set high

	constexpr MidiShort14 Combine14BitsMsbLsb(MidiByte7 msb, MidiByte7 lsb) { return (MidiShort14)((msb & 0x7F) << 8 | (lsb & 0x7F)); }

	constexpr MidiByte8 Msb(MidiShort16 number) { return (MidiByte8)(number & 0xFF00 >> 8); }
	constexpr MidiByte8 Lsb(MidiShort16 number) { return (MidiByte8)(number & 0x00FF); }

	constexpr MidiShort16 Msint16(MidiWord32 word) { return (MidiShort16)(word & 0xFFFF0000 >> 16); }
	constexpr MidiShort16 Lsint16(MidiWord32 word) { return (MidiShort16)(word & 0x0000FFFF); }

	constexpr uint8_t MostSignificantNibble(uint8_t b) { return (b & 0xF0) >> 4; }
	constexpr uint8_t LeastSignificantNibble(uint8_t b) { return b & 0x0F; }

	constexpr void SetLeastSignificantNibble(MidiByte8& b, const MidiNibble4 nibbleValue) { b = ((b & 0xF0) | (nibbleValue & 0x0F)); }
	constexpr void SetMostSignificantNibble(MidiByte8& b, const MidiNibble4 nibbleValue) { b = ((b & 0x0F) | (nibbleValue << 4)); }

}