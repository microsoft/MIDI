// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiEnumerator::impl
	{

		// device id, device info
		std::map<MidiObjectId, MidiTransportInformation> _transports;

		// device id, device info
		std::map<MidiObjectId, MidiDeviceInformation> _devices;

		// device id, endpoint id, endpoint info
		std::map<std::pair<MidiObjectId, MidiObjectId>, MidiEndpointInformation> _endpoints;

		// TODO: Will need to provide a hash function for the above. 
		// Could use boost hash_combine or hash_value for std::pair
		// old example: https://stackoverflow.com/questions/32685540/why-cant-i-compile-an-unordered-map-with-a-pair-as-key
	};

}