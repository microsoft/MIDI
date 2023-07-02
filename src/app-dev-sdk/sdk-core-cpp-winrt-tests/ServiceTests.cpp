#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>

using namespace winrt;
using namespace Microsoft::Devices::Midi2;


TEST_CASE("Check MIDI Services Properties")
{
	SECTION("Check for Windows MIDI Services")
	{
		REQUIRE_NOTHROW(MidiServices::CheckForWindowsMidiServices());
	}

	SECTION("Get installed Windows MIDI Services Version")
	{
		REQUIRE_NOTHROW(MidiServices::GetInstalledWindowsMidiServicesVersion());


		hstring version = MidiServices::GetInstalledWindowsMidiServicesVersion();

		std::cout << "Windows MIDI Services API/Service Version: " << winrt::to_string(version) << std::endl;

		REQUIRE(version != L"");
	}

	SECTION("Get SDK Version")
	{
		REQUIRE_NOTHROW(MidiServices::SdkVersion());

		hstring version = MidiServices::SdkVersion();

		std::cout << "SDK Version: " << winrt::to_string(version) << std::endl;

		REQUIRE(version != L"");
	}

	SECTION("Get Latest MIDI Services Installer URI")
	{
		REQUIRE_NOTHROW(MidiServices::LatestMidiServicesInstallUri());

		auto uri = MidiServices::LatestMidiServicesInstallUri();

		std::cout << "Installer URI: " << winrt::to_string(uri.ToString()) << std::endl;
	}

}


TEST_CASE("Access MIDI Services Transports")
{
	SECTION("Get installed transports")
	{
		REQUIRE_NOTHROW(MidiServices::GetInstalledTransports());

		auto transports = MidiServices::GetInstalledTransports();

		// zero installed transports is, in theory, a passing case, but we're going to ignore that
		REQUIRE(transports.Size() > 0);
	}

}
