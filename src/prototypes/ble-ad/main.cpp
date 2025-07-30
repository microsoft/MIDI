// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;

using namespace ::winrt::Windows::Devices::Bluetooth;
using namespace ::winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

#define MIDI_BLE_SERVICE_UUID                   L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}"
#define MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID    L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}"
#define MIDI_BLE_COMPANY_CODE_MICROSOFT         0xFE08



int main()
{
    winrt::init_apartment();

	auto result = GattServiceProvider::CreateAsync(winrt::guid(MIDI_BLE_SERVICE_UUID)).get();
	
	if (result.Error() != BluetoothError::Success)
	{
		std::cout << "Failed to create GATT service" << std::endl;

		return 1;
	}

	auto provider = result.ServiceProvider();

	auto properties = GattCharacteristicProperties::Read | GattCharacteristicProperties::WriteWithoutResponse | GattCharacteristicProperties::Notify;

	GattLocalCharacteristicParameters parameters;
	parameters.ReadProtectionLevel(GattProtectionLevel::Plain());
	parameters.WriteProtectionLevel(GattProtectionLevel::Plain());
	parameters.CharacteristicProperties(properties);
	parameters.UserDescription(L"Pete Windows Test");

	auto characteristicResult = provider.Service().CreateCharacteristicAsync(winrt::guid(MIDI_BLE_DATA_IO_CHARACTERISTIC_UUID), parameters).get();
	if (characteristicResult.Error() != BluetoothError::Success)
	{
		std::cout << "Failed to set IO characteristic" << std::endl;

		return 1;
	}

	// we have the characteristic, so set up the event handlers for it

	auto characteristic = characteristicResult.Characteristic();

	auto readRequestedHandler = [&](GattLocalCharacteristic const& sender, GattReadRequestedEventArgs const& args)
		{
			std::cout << "Read requested" << std::endl;
		};

	auto writeRequestedHandler = [&](GattLocalCharacteristic const& sender, GattWriteRequestedEventArgs const& args)
		{
			std::cout << "Write requested" << std::endl;
		};

	auto subscribedClientsChangedHandler = [&](GattLocalCharacteristic const& sender, IInspectable const& args)
		{
			std::cout << "Subscribed clients changed" << std::endl;
		};

	auto advertisementStatusChangedHandler = [&](GattServiceProvider const& sender, GattServiceProviderAdvertisementStatusChangedEventArgs const& args)
		{
			std::cout << "Advertisement status changed" << std::endl;
		};


	characteristic.ReadRequested(readRequestedHandler);
	characteristic.WriteRequested(writeRequestedHandler);
	characteristic.SubscribedClientsChanged(subscribedClientsChangedHandler);
	provider.AdvertisementStatusChanged(advertisementStatusChangedHandler);

	GattServiceProviderAdvertisingParameters adparams;
	adparams.IsConnectable(true);
	adparams.IsDiscoverable(true);

	provider.StartAdvertising(adparams);

	std::cout << "press enter to stop advertising" << std::endl;

	system("pause");


	provider.StopAdvertising();

}
