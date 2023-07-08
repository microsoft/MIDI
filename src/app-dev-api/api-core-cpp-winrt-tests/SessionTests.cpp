#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>

using namespace winrt;
using namespace Windows::Devices::Midi2;

// Note: These will be exposed by properties in the future

#define MIDI_SDK_LOOPBACK_SIM1_ENDPOINT_ID L"SWD\\MIDISDK\\MIDI_LOOPBACK1.LOOP20"
#define MIDI_SDK_LOOPBACK_SIM2_ENDPOINT_ID L"SWD\\MIDISDK\\MIDI_LOOPBACK2.LOOP20"

TEST_CASE("Create new session and endpoints")
{
	auto settings = MidiSessionSettings::Default();

	auto session = MidiSession::CreateNewSession(L"Test Session Name", settings);

	REQUIRE((bool)(session.IsOpen()));

	SECTION("Timestamps")
	{
		SECTION("MIDI timestamp appears to work")
		{
			std::cout << "Current timestamp: " << session.GetMidiTimestamp() << std::endl;
			std::cout << "Current timestamp: " << session.GetMidiTimestamp() << std::endl;
			std::cout << "Current timestamp: " << session.GetMidiTimestamp() << std::endl;
			std::cout << "Current timestamp: " << session.GetMidiTimestamp() << std::endl;
		}

		SECTION("MIDI frequency appears to work")
		{
			std::cout << "Timestamp Frequency: " << session.GetMidiTimestampFrequency() << std::endl;
		}

	}

	SECTION("Endpoints")
	{
//		hstring id1 = MIDI_SDK_LOOPBACK_SIM1_ENDPOINT_ID;
//		hstring id2 = MIDI_SDK_LOOPBACK_SIM2_ENDPOINT_ID;

		SECTION("Creating MIDI Services Sample Abstraction")
		{

			REQUIRE((bool)(session.Connections().Size() == 0));

			std::cout << "Connecting to Endpoint" << std::endl;

			auto conn1 = session.ConnectToEndpoint(L"foobarbaz", MidiEndpointConnectOptions::Default());

			REQUIRE((bool)(conn1 != nullptr));
			REQUIRE((bool)(session.Connections().Size() == 1));

			std::cout << "Endpoint 1 Id: " << winrt::to_string(conn1.DeviceId()) << std::endl;

			std::for_each(
				begin(session.Connections()),
				end(session.Connections()),
				[](const winrt::Windows::Foundation::Collections::IKeyValuePair<hstring, winrt::Windows::Devices::Midi2::MidiEndpointConnection>& kvp)
				{
					hstring key = kvp.Key();

					std::cout << "Endpoint Key in Map: " << winrt::to_string(key) << std::endl;
				}
			);



		}

	}

}

