// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>

#include "WindowsMidiServicesSession.h"
#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi
{

	struct MidiDevice::impl
	{
		MidiObjectId ParentSessionId;
		Enumeration::MidiDeviceInformation Information;

		// key is endpoint ID.
		std::map<MidiObjectId, MidiStream> _openStreams;

		//MidiDevice(Enumeration::MidiDeviceInformation information);

	};


	const Enumeration::MidiDeviceInformation MidiDevice::getInformation()
	{

	}

	const bool MidiDevice::getOpenStream(const MidiObjectId& streamId, MidiStream& stream)
	{

	}


	const MidiObjectId MidiDevice::getParentSessionID()
	{

	}


	MidiStreamOpenResult MidiDevice::OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{

	}

	MidiStreamOpenResult MidiDevice::OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options)
	{

	}

	MidiStreamOpenResult MidiDevice::OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{

	}

	MidiStreamOpenResult MidiDevice::OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options)
	{

	}

	bool MidiDevice::SendUmp(const Enumeration::MidiStreamInformation& information, const Messages::Ump& message)
	{

	}

	bool MidiDevice::SendUmp(const MidiObjectId& streamId, const Messages::Ump& message)
	{

	}

	void MidiDevice::Close()
	{

	}






}