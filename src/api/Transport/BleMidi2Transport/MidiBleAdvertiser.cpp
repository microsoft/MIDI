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

	ad.ServiceUuids().Append(winrt::guid(MidiBleProtocol::MidiServiceUuid));

	//foundation::MemoryBuffer buffer;
	//bt::Advertisement::BluetoothLEManufacturerData manufacturer(MidiBleProtocol::MicrosoftBluetoothCompanyCode, buffer);
	
	bt::Advertisement::BluetoothLEAdvertisementPublisher publisher(ad);

	m_publisher = publisher;
	m_publisher.Start();

	return S_OK;
}