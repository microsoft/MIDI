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

	struct MidiEnumerator::implMidiEnumerator
	{
	public:
		// device id, device info
		std::map<MidiObjectId, MidiTransportInformation> _transports{};

		// device id, device info
		std::map<MidiObjectId, MidiDeviceInformation> _devices{};

		// device id, endpoint id, endpoint info
		std::map<std::pair<MidiObjectId, MidiObjectId>, MidiStreamInformation> _streams{};

		// TODO: Will need to provide a hash function for the above. 
		// Could use boost hash_combine or hash_value for std::pair
		// old example: https://stackoverflow.com/questions/32685540/why-cant-i-compile-an-unordered-map-with-a-pair-as-key
	};



	MidiEnumerator::MidiEnumerator()
	{
		_pimpl = new implMidiEnumerator;
	}

	MidiEnumerator::~MidiEnumerator()
	{
		delete _pimpl;
	}


	bool MidiEnumerator::Load()
	{
		return false;
	}

	MidiEnumeratorCreateResult MidiEnumerator::Create(bool skipEnumeration)
	{
		return MidiEnumeratorCreateResult{};
	}


	const bool MidiEnumerator::GetTransportInformationFromId(MidiObjectId transportId, MidiTransportInformation& info)
	{
		return false;
	}

	const bool MidiEnumerator::GetDeviceInformationFromId(MidiObjectId deviceId, MidiDeviceInformation& info)
	{
		return false;
	}

	const bool MidiEnumerator::GetStreamInformationFromId(MidiObjectId deviceId, MidiObjectId streamId, MidiStreamInformation& info)
	{
		return false;
	}


	const MidiTransportInformationCollection MidiEnumerator::GetStaticTransportList()
	{
		return MidiTransportInformationCollection{};
	}

	const MidiDeviceInformationCollection MidiEnumerator::GetStaticDeviceList()
	{
		return MidiDeviceInformationCollection{};
	}
	const MidiDeviceInformationCollection MidiEnumerator::GetStaticDeviceListByName(wchar_t* name)
	{
		return MidiDeviceInformationCollection{};
	}
	const MidiDeviceInformationCollection MidiEnumerator::GetStaticDeviceListByDeviceSuppliedName(wchar_t* deviceSuppliedDeviceName)
	{
		return MidiDeviceInformationCollection{};
	}

	const MidiStreamInformationCollection MidiEnumerator::GetStaticStreamList()
	{
		return MidiStreamInformationCollection{};
	}
	const MidiStreamInformationCollection MidiEnumerator::GetStaticStreamList(MidiObjectId deviceId)
	{
		return MidiStreamInformationCollection{};
	}
	const MidiStreamInformationCollection MidiEnumerator::GetStaticStreamListByName(wchar_t* name)
	{
		return MidiStreamInformationCollection{};
	}
	const MidiStreamInformationCollection MidiEnumerator::GetStaticStreamListByDeviceSuppliedName(wchar_t* deviceSuppliedStreamName)
	{
		return MidiStreamInformationCollection{};
	}


	void MidiEnumerator::SubscribeToTransportChangeNotifications(
		const MidiTransportAddedCallback& addedCallback,
		const MidiTransportRemovedCallback& removedCallback,
		const MidiTransportChangedCallback& changedCallback)
	{

	}

	void MidiEnumerator::SubscribeToDeviceChangeNotifications(
		const MidiDeviceAddedCallback& addedCallback,
		const MidiDeviceRemovedCallback& removedCallback,
		const MidiDeviceChangedCallback& changedCallback)
	{

	}

	void MidiEnumerator::SubscribeToEndpointChangeNotifications(
		const MidiStreamAddedCallback& addedCallback,
		const MidiStreamRemovedCallback& removedCallback,
		const MidiStreamChangedCallback& changedCallback)
	{

	}





}
