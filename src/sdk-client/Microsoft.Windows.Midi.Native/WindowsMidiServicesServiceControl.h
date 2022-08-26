// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

#pragma once

#include <Windows.h>
#include <stdint.h>

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

namespace Microsoft::Windows::Midi //::inline v0_1_0_pre
{
	// ----------------------------------------------------------------------------
	// Service-specific information and configuration
	// ----------------------------------------------------------------------------


	enum WINDOWSMIDISERVICES_API MidiServicePingResponse
	{
		MidiServicePingPong,
		MidiServicePingTimeOut,
		MidiServicePingError
	};


	struct WINDOWSMIDISERVICES_API MidiServicesComponentVersion
	{
		int32_t Major;
		int32_t Minor;
		int32_t Build;
		int32_t Revision;
	};

	class WINDOWSMIDISERVICES_API MidiService final
	{
	private:
		MidiService();								// static only
		MidiService(const MidiService& service);	// no copying
	public:

		static MidiServicesComponentVersion getClientApiVersion;

		// returns true if the Windows MIDI Services service is installed
		static const bool IsInstalled();

		// returns true if the SCM reports that the services are running
		// It doesn't tell you if it's functioning properly
		static const bool IsRunning();

		// start the MIDI Service if it's not running. Should be unnecessary in most cases
		static const bool Start();

		// pings the service. This will tell you if it is processing messages.
		// returns (out param) the version of the service which is running
		static const MidiServicePingResponse Ping(MidiServicesComponentVersion& serverVersionReported);

		// Return the URI of the latest installer for cases when the app needs to prompt the user to install
		static const char8_t* getServicesInstallerUri();

		// Returns the full file name, including the path, for an icon. Useful
		// for apps which want to show the icon in the app
		static const wchar_t* BuildFullPathDeviceIconFilename(const wchar_t* iconFileName);
		static const wchar_t* BuildFullPathEndpointIconFilename(const wchar_t* iconFileName);
		static const wchar_t* BuildFullPathTransportIconFilename(const wchar_t* iconFileName);
	};

}
