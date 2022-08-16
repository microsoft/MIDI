// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi
{

	struct MidiDevice::impl
	{
		Enumeration::MidiDeviceInformation Information;

		std::vector<MidiEndpoint> _openEndpoints;

		//MidiDevice(Enumeration::MidiDeviceInformation information);

	};

}