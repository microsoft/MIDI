#include "pch.h"

#include "catch_amalgamated.hpp"

#include <iostream>

using namespace winrt;
using namespace Microsoft::Devices::Midi2;


TEST_CASE("Check for Windows MIDI Services", "[MidiServices.CheckForWindowsMidiServices]")
{
	REQUIRE_NOTHROW(MidiServices::CheckForWindowsMidiServices());
}

TEST_CASE("Get installed Windows MIDI Services Version", "[MidiServices.GetInstalledWindowsMidiServicesVersion]")
{
	REQUIRE_NOTHROW(MidiServices::GetInstalledWindowsMidiServicesVersion());


	hstring version = MidiServices::GetInstalledWindowsMidiServicesVersion();

	REQUIRE(version != L"");
}

TEST_CASE("Get SDK Version", "[MidiServices.GetSdkVersion]")
{
	REQUIRE_NOTHROW(MidiServices::SdkVersion());

	hstring version = MidiServices::SdkVersion();

	REQUIRE(version != L"");
}

TEST_CASE("Get Latest MIDI Services Installer URI", "[MidiServices.LatestMidiServicesInstallUri]")
{
	REQUIRE_NOTHROW(MidiServices::LatestMidiServicesInstallUri());

	auto uri = MidiServices::LatestMidiServicesInstallUri();

	std::cout << uri.ToString().c_str();
}

TEST_CASE("Get Installed Transports", "[MidiServices.GetInstalledTransports]")
{
	REQUIRE_NOTHROW(MidiServices::GetInstalledTransports());

	auto transports = MidiServices::GetInstalledTransports();

	// zero installed transports is, in theory, a passing case, but we're going to ignore that
	REQUIRE(transports.Size() > 0);
}
