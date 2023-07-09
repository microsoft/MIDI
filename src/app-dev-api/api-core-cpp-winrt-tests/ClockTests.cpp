#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>

using namespace winrt;
using namespace Windows::Devices::Midi2;


TEST_CASE("Clock")
{
	SECTION("MIDI timestamp appears to work")
	{
		std::cout << "Current timestamp: " << MidiClock::GetMidiTimestamp() << std::endl;
		std::cout << "Current timestamp: " << MidiClock::GetMidiTimestamp() << std::endl;
		std::cout << "Current timestamp: " << MidiClock::GetMidiTimestamp() << std::endl;
		std::cout << "Current timestamp: " << MidiClock::GetMidiTimestamp() << std::endl;
	}

	SECTION("MIDI frequency appears to work")
	{
		std::cout << "Timestamp Frequency: " << MidiClock::GetMidiTimestampFrequency() << std::endl;
	}

}
