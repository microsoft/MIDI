#pragma once

#include "pch.h"

#include <queue>
#include <chrono>
#include "NetworkMidiCommandPacket.h"


struct NetworkMidiHostSession
{
private:

public:
	std::queue<NetworkMidiCommandPacket> IncomingFromNetworkCommandPacketQueue;
	std::queue<NetworkMidiCommandPacket> OutgoingToNetworkCommandPacketQueue;

	// incoming and outgoing MIDI messages
	//std::queue<uint32_t> IncomingFromNetworkUmpQueue;
	std::queue<uint32_t> OutgoingToNetworkUmpQueue;

	std::chrono::steady_clock::time_point LastIncomingMessageTime;

	networking::HostName ConnectedClient=0;
	winrt::hstring ConnectedPort;
	winrt::hstring ClientEndpointName;

	//sock::DatagramSocket socket;
	//NetworkMidiHostSession(sock::DatagramSocket socket);

	// these queues will also need to allow for re-sending previous MIDI messages 
	// (not command packets) when requested. Lots of ways to do that, but they'll 
	// require a change in how the messages are buffered, or require an additional
	// list of a certain size. Right now, this is not handled.
	// perhaps a small boost circular queue of a fixed size that contains the last X UMPs with their sequence numbers?


	// TODO: two threads. One for reading from network queue, one for writing to network queue

	winrt::Windows::Foundation::IAsyncAction StartAsync(networking::HostName connectedClient, winrt::hstring connectedPort);
	void End();


};
