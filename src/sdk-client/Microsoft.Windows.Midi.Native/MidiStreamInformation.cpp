// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>

#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiStreamInformation::impl
	{

	};

	MidiStreamInformation::MidiStreamInformation()
	{
		_pimpl = new impl;
	}

	MidiStreamInformation::~MidiStreamInformation()
	{
		delete _pimpl;
	}

	MidiStreamInformation::MidiStreamInformation(const MidiStreamInformation& info)
	{
		_pimpl = nullptr;
	}



	const MidiObjectId MidiStreamInformation::getId()
	{
		return MidiObjectId{};
	}

	const MidiObjectId MidiStreamInformation::getParentDeviceId()
	{
		return MidiObjectId{};
	}

	const MidiStreamType MidiStreamInformation::getStreamType()
	{
		return MidiStreamType::MidiStreamTypeBidirectional; // todo
	}

	const char8_t* MidiStreamInformation::getName()
	{
		return nullptr;
	}

	const char8_t* MidiStreamInformation::getDeviceSuppliedName()
	{
		return nullptr;
	}

	const wchar_t* MidiStreamInformation::getIconFileName()
	{
		return nullptr;
	}

	const wchar_t* MidiStreamInformation::getDescription()
	{
		return nullptr;
	}

}

