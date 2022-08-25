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

namespace Microsoft::Windows::Midi::Enumeration
{

	struct MidiDeviceInformation::impl
	{
		int TODO;
	};

	MidiDeviceInformation::MidiDeviceInformation()
	{
		_pimpl = new impl;
	}

	MidiDeviceInformation::~MidiDeviceInformation()
	{
		delete _pimpl;
	}

	MidiDeviceInformation::MidiDeviceInformation(const MidiDeviceInformation& info)
	{
		_pimpl = nullptr;
	}


	MidiObjectId MidiDeviceInformation::getId()
	{
		return MidiObjectId{};
	}

	MidiObjectId MidiDeviceInformation::getTransportId()
	{
		return MidiObjectId{};
	}

	const char8_t* MidiDeviceInformation::getName()
	{
		return nullptr;
	}

	const char8_t* MidiDeviceInformation::getDeviceSuppliedName()
	{
		return nullptr;
	}

	const char8_t* MidiDeviceInformation::getSerial()
	{
		return nullptr;
	}

	const wchar_t* MidiDeviceInformation::getIconFileName()
	{
		return nullptr;
	}

	const wchar_t* MidiDeviceInformation::getDescription()
	{
		return nullptr;
	}

	const bool MidiDeviceInformation::getIsRuntimeCreated()
	{
		return false;
	}

	const uint16_t MidiDeviceInformation::getOwningProcessIdIfRuntimeCreated()
	{
		return 0;
	}


}