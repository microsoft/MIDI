// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>
#include <assert.h>

#include "WindowsMidiServicesSession.h"
#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi
{
	struct MidiStream::implMidiStream
	{
	public:
		MidiObjectId StreamInformationId;
		MidiObjectId ParentDeviceInformationId;
		MidiStreamOpenOptions Options;

		bool SchedulingEnabled = false;
		bool HasReadQueue = false;
		bool HasWriteQueue = false;

		MidiMessagesReceivedCallback _messagesReceivedCallback = nullptr;

		implMidiStream(MidiObjectId streamId, MidiObjectId parentDeviceId, MidiStreamOpenOptions options)
		{
			StreamInformationId = streamId;
			ParentDeviceInformationId = parentDeviceId;
			Options = options;
		}
	};



	MidiStream::MidiStream(MidiObjectId streamId, MidiObjectId parentDeviceId, MidiStreamOpenOptions options)
	{
		_pimpl = new implMidiStream(streamId, parentDeviceId, options);

		// TODO: set flags based on options
	}

	MidiStream::~MidiStream()
	{
		delete _pimpl;
	}


	void MidiStream::Close()
	{
		// TODO: clean up

	}

	const MidiObjectId MidiStream::getStreamInformationId()
	{
		assert(_pimpl != nullptr);

		return _pimpl->StreamInformationId;
	}

	const MidiObjectId MidiStream::getParentDeviceInformationId()
	{
		assert(_pimpl != nullptr);

		return _pimpl->ParentDeviceInformationId;
	}

		// send a UMP with no scheduling. 
	bool MidiStream::SendUmp(const Messages::Ump& message)
	{
		return false;
	}

		// send a UMP with scheduling. Only works if the stream was created with that option
	bool MidiStream::SendUmp(MidiMessageTimestamp sendTimestamp, const Messages::Ump& message)
	{
		return false;
	}


}