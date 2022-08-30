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

	struct MidiStreamInformation::implMidiEndpointInformation
	{
	public:
		MidiObjectId _id;
		MidiObjectId _parentDeviceId;
		MidiEndpointType _endpointType;
		std::unique_ptr<std::wstring> Name;
		std::unique_ptr<std::wstring> DeviceSuppliedName;
		std::unique_ptr<std::wstring> IconFileName;
		std::unique_ptr<std::wstring> Description;

		implMidiEndpointInformation(MidiObjectId id, MidiObjectId parentDeviceId, MidiStreamType endpointType)
		{
			_id = id;
			_parentDeviceId = parentDeviceId;
			_streamType = streamType;
		}
	};

	MidiEndpointInformation::MidiEndpointInformation(MidiObjectId id, MidiObjectId parentDeviceId, MidiEndpointType endpointType)
	{
		_pimpl = new implMidiEndpointInformation(id, parentDeviceId, endpointType);
	}

	MidiEndpointInformation::~MidiEndpointInformation()
	{
		delete _pimpl;
	}

	MidiEndpointInformation::MidiEndpointInformation(const MidiEndpointInformation& info)
	{
		_pimpl = nullptr;
	}



	const MidiObjectId MidiEndpointInformation::getId()
	{
		// TODO
		return MidiObjectId{};
	}

	const MidiObjectId MidiEndpointInformation::getParentDeviceId()
	{
		// TODO
		return MidiObjectId{};
	}

	const MidiStreamType MidiEndpointInformation::getStreamType()
	{
		// TODO
		return MidiStreamType::MidiStreamTypeBidirectional; // todo
	}

	const wchar_t* MidiEndpointInformation::getName()
	{
		// TODO
		return nullptr;
	}

	const wchar_t* MidiEndpointInformation::getDeviceSuppliedName()
	{
		// TODO
		return nullptr;
	}

	const wchar_t* MidiEndpointInformation::getIconFileName()
	{
		// TODO
		return nullptr;
	}

	const wchar_t* MidiEndpointInformation::getDescription()
	{
		// TODO
		return nullptr;
	}

}

