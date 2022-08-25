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
		Enumeration::MidiDeviceInformation Information{};

		// key is endpoint ID.
		std::map<MidiObjectId, MidiStream> _openStreams{};

		//MidiDevice(Enumeration::MidiDeviceInformation information);

		friend class MidiDevice;
	};

	MidiDevice::MidiDevice()
	{
		_pimpl = new impl;
	}

	MidiDevice::~MidiDevice()
	{
		delete _pimpl;
	}




	const Enumeration::MidiDeviceInformation MidiDevice::getInformation()
	{
		return _pimpl->Information;
	}

	const bool MidiDevice::getOpenStream(const MidiObjectId& streamId, MidiStream& stream)
	{
		return false;
	}


	const MidiObjectId MidiDevice::getParentSessionID()
	{
		return MidiObjectId{};
	}


	MidiStreamOpenResult MidiDevice::OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{
		return MidiStreamOpenResult{};
	}

	MidiStreamOpenResult MidiDevice::OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options)
	{
		return MidiStreamOpenResult{};
	}

	MidiStreamOpenResult MidiDevice::OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback)
	{
		return MidiStreamOpenResult{};
	}

	MidiStreamOpenResult MidiDevice::OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options)
	{
		return MidiStreamOpenResult{};
	}

	bool MidiDevice::SendUmp(const Enumeration::MidiStreamInformation& information, const Messages::Ump& message)
	{
		return false;
	}

	bool MidiDevice::SendUmp(const MidiObjectId& streamId, const Messages::Ump& message)
	{
		return false;
	}

	void MidiDevice::Close()
	{

	}






}