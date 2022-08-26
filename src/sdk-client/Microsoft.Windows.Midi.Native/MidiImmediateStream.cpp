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

	class MidiImmediateStream::implMidiImmediateStream
	{
	public:
		Enumeration::MidiStreamInformation Information;
		MidiMessagesReceivedCallback _messagesReceivedCallback;

		//MidiStream(const Enumeration::MidiStreamInformation& information, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		//MidiStream(const Enumeration::MidiStreamInformation& information);

		// TODO: Vector of groups / channels / protocol versions / other MIDI CI information
	};








}