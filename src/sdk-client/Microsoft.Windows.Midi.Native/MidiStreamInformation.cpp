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

	struct MidiStreamInformation::implMidiStreamInformation
	{
	public:
		MidiObjectId _id;
		MidiObjectId _parentDeviceId;
		MidiStreamType _streamType;
		std::unique_ptr<std::wstring> Name;
		std::unique_ptr<std::wstring> DeviceSuppliedName;
		std::unique_ptr<std::wstring> IconFileName;
		std::unique_ptr<std::wstring> Description;

		implMidiStreamInformation(MidiObjectId id, MidiObjectId parentDeviceId, MidiStreamType streamType)
		{
			_id = id;
			_parentDeviceId = parentDeviceId;
			_streamType = streamType;
		}
	};

	MidiStreamInformation::MidiStreamInformation(MidiObjectId id, MidiObjectId parentDeviceId, MidiStreamType streamType)
	{
		_pimpl = new implMidiStreamInformation(id, parentDeviceId, streamType);
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

	const wchar_t* MidiStreamInformation::getName()
	{
		return nullptr;
	}

	const wchar_t* MidiStreamInformation::getDeviceSuppliedName()
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

