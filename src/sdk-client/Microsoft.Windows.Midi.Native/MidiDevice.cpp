// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <assert.h>

#include "WindowsMidiServicesSession.h"
#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi
{

	struct MidiDevice::implMidiDevice
	{
	public:

		MidiObjectId ParentSessionId;
		MidiObjectId DeviceId;

		// key is endpoint ID.
		std::map<MidiObjectId, MidiStream> _openStreams{};

		//MidiDevice(Enumeration::MidiDeviceInformation information);

		implMidiDevice(MidiObjectId deviceId, MidiObjectId& parentSessionId)
		{
			DeviceId = deviceId;
			ParentSessionId = parentSessionId;
		}
	};

	MidiDevice::MidiDevice(MidiObjectId deviceId, MidiObjectId& parentSessionId)
	{
		_pimpl = new implMidiDevice(deviceId, parentSessionId);
	}

	MidiDevice::~MidiDevice()
	{
		delete _pimpl;
	}


	const bool MidiDevice::getOpenedStream(const MidiObjectId& streamId, MidiStream& stream)
	{
		return false;
	}


	const MidiObjectId MidiDevice::getParentSessionID()
	{
		return MidiObjectId{};
	}



	MidiStreamOpenResult MidiDevice::OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{
		return MidiStreamOpenResult{};
	}

	MidiStreamOpenResult MidiDevice::OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options)
	{
		return MidiStreamOpenResult{};
	}

	void MidiDevice::Close()
	{

	}






}