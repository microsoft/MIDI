// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>

#include "WindowsMidiServicesServiceControl.h"


namespace Microsoft::Windows::Midi
{
	MidiService::MidiService()
	{
		// private constructor
	}

	MidiService::MidiService(const MidiService& service)
	{
		// no copying
	}

	const bool MidiService::IsInstalled()
	{
		// checks if SCM reports the service exists
		return true;
	}

	const bool MidiService::IsRunning()
	{
		// checks if SCM reports the service is running
		return true;
	}

	const bool MidiService::Start()
	{
		// use SCM to start the service
		return true;
	}

	const MidiServicePingResponse MidiService::Ping(MidiServicesComponentVersion& serverVersionReported)
	{
		// send a ping message to the service to verify it's working
		return MidiServicePingResponse::MidiServicePingTimeOut;
	}

	const char8_t* MidiService::getServicesInstallerUri()
	{
		// Return the URI of the latest installer for cases when the app needs to prompt the user to install
		return nullptr;
	}


	const wchar_t* MidiService::BuildFullPathDeviceIconFilename(const wchar_t* iconFileName)
	{
		// uses the known paths to get the full path for the icon. Used for display
		return nullptr;
	}

	const wchar_t* MidiService::BuildFullPathEndpointIconFilename(const wchar_t* iconFileName)
	{
		// uses the known paths to get the full path for the icon. Used for display
		return nullptr;
	}

	const wchar_t* MidiService::BuildFullPathTransportIconFilename(const wchar_t* iconFileName)
	{
		// uses the known paths to get the full path for the icon. Used for display
		return nullptr;
	}

}