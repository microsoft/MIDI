#include "pch.h"
#include "NetworkHostAdvertiser.h"

#include <iostream>

	winrt::Windows::Foundation::IAsyncOperation<bool> NetworkHostAdvertiser::AdvertiseAsync(
		winrt::hstring const serviceInstanceNameWithoutSuffix,
		const networking::HostName& hostName,
		const networking::Sockets::DatagramSocket& boundSocket,
		uint16_t const port,
		winrt::hstring const midiEndpointName,
		winrt::hstring const midiProductInstanceId)

	{

		//if (_services.find(serviceInstanceNameWithoutSuffix) != _services.end())
		//{
		//	// entry already exists and is advertising

		//	// should report this somehow
		//	co_return false;
		//}

		std::cout << "Creating dnssd instance" << std::endl;

		auto fullServiceName = BuildFullServiceInstanceName(serviceInstanceNameWithoutSuffix);

		std::cout << "Service: " << winrt::to_string(fullServiceName) << std::endl;

		// create the service instance
		auto service = dnssd::DnssdServiceInstance(
			fullServiceName,
			hostName,
			port);


		std::cout << "Adding TXT" << std::endl;

		// add our txt attributes. Both are optional, but we should always
		// include at least the endpoint name
		service.TextAttributes().Insert(L"EndpointName", midiEndpointName);

		if (!midiProductInstanceId.empty())
		{
			service.TextAttributes().Insert(L"ProductInstanceId", midiProductInstanceId);
		}


		std::cout << "Registering DNSSD" << std::endl;

		// once we have the service instance, register the socket
		// we expect the socket to already be bound to the port, as
		// advertising is an optional step
		dnssd::DnssdRegistrationResult registration = co_await service.RegisterDatagramSocketAsync(boundSocket);

		if (registration.Status() == dnssd::DnssdRegistrationStatus::Success)
		{
			std::cout << "Registration success" << std::endl;

			// we're good to go

			// TODO: add to our services map (if we decide to keep that tracking here)

			co_return true;
		}
		else
		{
			std::cout << "Registration failure" << std::endl;

			// TODO: should log this failure

			co_return false;
		}

	}
