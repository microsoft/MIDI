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

	struct MidiTransportInformation::impl
	{

	};


	MidiTransportInformation::MidiTransportInformation()
	{
		_pimpl = new impl;
	}

	MidiTransportInformation::~MidiTransportInformation()
	{
		delete _pimpl;
	}

	MidiTransportInformation::MidiTransportInformation(const MidiTransportInformation& info)
	{
		_pimpl = nullptr;
	}


	const MidiObjectId MidiTransportInformation::getId()
	{
		return MidiObjectId{};
	}

	const char8_t* MidiTransportInformation::getName()
	{
		return nullptr;
	}

	const char8_t* MidiTransportInformation::getLongName()
	{
		return nullptr;

	}

	const wchar_t* MidiTransportInformation::getIconFileName()
	{
		return nullptr;
	}

	const bool MidiTransportInformation::getSupportsRuntimeDeviceCreation()
	{
		return false;
	}


}