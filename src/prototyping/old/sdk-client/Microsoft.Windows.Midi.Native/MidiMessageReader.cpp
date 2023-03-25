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

	struct MidiMessageReader::implMidiMessageReader
	{

	};

	MidiMessageReader::MidiMessageReader()
	{
		_pimpl = new implMidiMessageReader;
	}

	MidiMessageReader::~MidiMessageReader()
	{
		delete _pimpl;
	}

	bool MidiMessageReader::eof()
	{
		// return true if queue is empty
	}

	int MidiMessageReader::GetNextUmpWordCount()
	{
		// reads the first nibble of the next word, and then uses 
		// UMP message type to calc number of words
	}


	bool ReadUmp32(Messages::Ump32& message, bool validate)
	{
		// read next word from the queue 
		return false;
	}

	bool ReadUmp64(Messages::Ump64& message, bool validate)
	{
		// read next 2 words from the queue 
		return false;
	}

	bool ReadUmp96(Messages::Ump96& message, bool validate)
	{
		// read next 3 words from the queue 
		return false;
	}

	bool ReadUmp128(Messages::Ump128& message, bool validate)
	{
		// read next 4 words from the queue 
		return false;
	}


}