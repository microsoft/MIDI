#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>
#include <algorithm>
#include <Windows.h>

using namespace winrt;
using namespace Windows::Devices::Midi2;


TEST_CASE("Connected.Session.CreateSession Create new session")
{
	hstring sessionName = L"Test Session Name";

	auto settings = MidiSessionSettings::Default();
	auto session = MidiSession::CreateSession(sessionName, settings);

	REQUIRE((bool)(session != nullptr));

	REQUIRE((bool)(session.IsOpen()));

	REQUIRE((bool)(session.Name() == sessionName));

	REQUIRE((bool)(session.Id().empty() == false));

	REQUIRE((bool)(session.Connections().Size() == 0));

	session.Close();
}