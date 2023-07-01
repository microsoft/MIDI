#include "pch.h"

#include "catch_amalgamated.hpp"

using namespace winrt;
using namespace Microsoft::Devices::Midi2;


TEST_CASE("Create session")
{
	auto settings = MidiSessionSettings::Default();

	auto session = MidiSession::CreateNewSession(L"Test Session Name", settings);


	REQUIRE((bool)(session.IsOpen()));
}

