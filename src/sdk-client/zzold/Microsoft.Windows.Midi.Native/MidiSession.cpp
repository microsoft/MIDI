// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#define WINDOWSMIDISERVICES_EXPORTS

#include <string>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>

#include "WindowsMidiServicesSession.h"
#include "WindowsMidiServicesEnumeration.h"

namespace Microsoft::Windows::Midi
{
	struct MidiSession::implMidiSession
	{
	public:
		MidiObjectId Id;
		std::unique_ptr<std::wstring> Name;
		SYSTEMTIME CreatedDateTime;

		std::map<MidiObjectId, MidiDevice> _openDevices;
	};

	MidiSession::MidiSession()
	{
		_pimpl = new implMidiSession;
	}

	MidiSession::~MidiSession()
	{
		delete _pimpl;
	}


	const MidiObjectId MidiSession::getId()
	{
		return _pimpl->Id;
	}

	const SYSTEMTIME MidiSession::getCreatedDateTime()
	{
		return _pimpl->CreatedDateTime;
	}

	const wchar_t* MidiSession::getName()
	{
		return _pimpl->Name->c_str();
	}

	void MidiSession::UpdateName(const wchar_t* newName)
	{
		// TODO: Service call
	}

	const MidiDevice* MidiSession::GetOpenedDevice(MidiObjectId id)
	{
		// TODO: get device from the internal map
		return nullptr;
	}

	MidiDeviceOpenResult MidiSession::OpenDevice(const MidiObjectId& deviceId, const MidiDeviceOpenOptions& options)
	{
		// TODO: Service call
		return MidiDeviceOpenResult{};
	}

	MidiDeviceOpenResult MidiSession::OpenDevice(const MidiObjectId& deviceId)
	{
		// TODO: Service call

		return MidiDeviceOpenResult{};
	}




}