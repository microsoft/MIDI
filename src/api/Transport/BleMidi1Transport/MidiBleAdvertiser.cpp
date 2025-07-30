// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"


HRESULT MidiBleAdvertiser::Initialize(

)
{


	return S_OK;
}

HRESULT MidiBleAdvertiser::Advertise(

)
{
	bt::Advertisement::BluetoothLEAdvertisement ad{};

	ad.ServiceUuids().Append(winrt::guid(MIDI_BLE_SERVICE_UUID));

	//foundation::MemoryBuffer buffer;
	//bt::Advertisement::BluetoothLEManufacturerData manufacturer(static_cast<uint16_t>(MIDI_BLE_COMPANY_CODE_MICROSOFT), buffer);
	
	bt::Advertisement::BluetoothLEAdvertisementPublisher publisher(ad);

	m_publisher = publisher;
	m_publisher.Start();

	return S_OK;
}