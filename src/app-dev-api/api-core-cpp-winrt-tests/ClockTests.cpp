// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>

using namespace winrt;
using namespace Windows::Devices::Midi2;


TEST_CASE("Offline.Clock.Timestamp Clock Basics")
{
	SECTION("MIDI timestamp appears to work")
	{
		for (int i = 0; i < 20; i++)
		{
			std::cout << "Timestamp: " << i << " : " << MidiClock::GetMidiTimestamp() << std::endl;
		}
	}

	SECTION("MIDI frequency appears to work")
	{
		std::cout << "Timestamp Frequency: " << MidiClock::GetMidiTimestampFrequency() << std::endl;
	}

}
