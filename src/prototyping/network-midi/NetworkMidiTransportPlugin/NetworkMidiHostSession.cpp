#include "pch.h"

#include "NetworkMidiHostSession.h"


winrt::Windows::Foundation::IAsyncAction NetworkMidiHostSession::StartAsync(networking::HostName connectedClient, winrt::hstring connectedPort)
{
	co_return;
}

void NetworkMidiHostSession::End()
{
	// send "bye"
		 
	// Stop the read/write threads
}