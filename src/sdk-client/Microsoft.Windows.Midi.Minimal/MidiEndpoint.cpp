// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS


#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi
{
	struct MidiEndpoint::impl
	{
		Enumeration::MidiEndpointInformation Information;
		MidiMessagesReceivedCallback _messagesReceivedCallback;

		//MidiEndpoint(const Enumeration::MidiEndpointInformation& information, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		//MidiEndpoint(const Enumeration::MidiEndpointInformation& information);

		// TODO: Vector of groups / channels / protocol versions / other MIDI CI information
	};

}