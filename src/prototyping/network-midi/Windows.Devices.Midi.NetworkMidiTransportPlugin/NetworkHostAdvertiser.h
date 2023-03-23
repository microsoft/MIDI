#pragma once

#include "pch.h"

#include <string>

class NetworkHostAdvertiser
{
private:
	const winrt::hstring _serviceInstanceNameSuffix = L"._midi2._udp.local";

	// TODO: Keep a list of DnssdServiceInstance objects here, should we decide to track
	//	std::unordered_map<winrt::hstring, winrt::hstring> _services;


	inline const winrt::hstring BuildFullServiceInstanceName(const winrt::hstring nameWithoutSuffix)
	{
		return nameWithoutSuffix + _serviceInstanceNameSuffix;
	}

public:
	winrt::Windows::Foundation::IAsyncOperation<bool>  AdvertiseAsync(
		winrt::hstring const serviceInstanceNameWithoutSuffix,
		const networking::HostName& hostName,
		const networking::Sockets::DatagramSocket& boundSocket,
		uint16_t const port,
		winrt::hstring const midiEndpointName,
		winrt::hstring const midiProductInstanceId);



};
