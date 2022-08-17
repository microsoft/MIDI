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
	struct MidiStream::impl
	{
		Enumeration::MidiStreamInformation Information;
		MidiMessagesReceivedCallback _messagesReceivedCallback;

		//MidiStream(const Enumeration::MidiStreamInformation& information, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		//MidiStream(const Enumeration::MidiStreamInformation& information);

		// TODO: Vector of groups / channels / protocol versions / other MIDI CI information
	};








	bool MidiStream::SendUmp(const Messages::Ump& message)
	{

	}

	void MidiStream::Close()
	{

	}
}