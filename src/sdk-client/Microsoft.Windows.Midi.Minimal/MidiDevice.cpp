// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>

#include "WindowsMidiServicesClient.h"

namespace Microsoft::Windows::Midi
{

	struct MidiDevice::impl
	{
		MidiObjectId ParentSessionId;
		Enumeration::MidiDeviceInformation Information;

		// key is endpoint ID.
		std::map<MidiObjectId, MidiEndpoint> _openEndpoints;

		//MidiDevice(Enumeration::MidiDeviceInformation information);

	};


	const Enumeration::MidiDeviceInformation MidiDevice::getInformation()
	{

	}

	const bool MidiDevice::getOpenEndpoint(const MidiObjectId& endpointId, MidiEndpoint& endpoint)
	{

	}


	const MidiObjectId MidiDevice::getParentSessionID()
	{

	}


	MidiEndpoint MidiDevice::OpenEndpoint(const Enumeration::MidiEndpointInformation& endpointInformation, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{

	}

	MidiEndpoint MidiDevice::OpenEndpoint(const Enumeration::MidiEndpointInformation& endpointInformation, const MidiEndpointOpenOptions options)
	{

	}

	MidiEndpoint MidiDevice::OpenEndpoint(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{

	}

	MidiEndpoint MidiDevice::OpenEndpoint(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options)
	{

	}

	bool MidiDevice::SendUmp(const Enumeration::MidiEndpointInformation& information, const Messages::Ump& message)
	{

	}

	bool MidiDevice::SendUmp(const MidiObjectId& endpointId, const Messages::Ump& message)
	{

	}

	void MidiDevice::Close()
	{

	}






}