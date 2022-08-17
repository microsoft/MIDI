#pragma once

#include <Windows.h>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>




class MidiEndpointBidirectionalMessageQueue
{
protected:
	GUID _endpointId;
	GUID _sessionId;

	boost::interprocess::message_queue _queueOutToEndpoint;
	boost::interprocess::message_queue _queueInFromEndPoint;

	const unsigned int _defaultPriority = 0;
	bool _connected = false;



public:
	// TODO: May need to prepend a send/receive timestamp to incoming and outgoing
	// messages, adding to the overall size. This is not MIDI protocol, but is
	// internal implementation. second or third most important request from
	// app devs is service-side scheduling of message sending

	// TODO: Methods for sending, receiving, processing messages, etc.
	// This queue may work on complete messages instead of words, depending
	// upon which is more efficient in this case. (We'll have full messages
	// on the server, after processing them)
	// But the queue used for talking to the endpoint from the service likely
	// needs to work on just words

	// also need some x-process event handliner here so service can notify client
	// that there are new incoming messages, and so client can notify service that
	// there are messages to send out

	// may eventually want to consider a solution to space-wasting. Most MIDI messages
	// especially early on, will be 32 bit packets. Having to allocate a fixed 128
	// bits for each one wastes a lot of space. But having a queue of words
	// is not efficient in the boost implementation as you end up with a huge index
	// and a lot of work when it wraps around. Will need to test performance.
	// One option may be to have a separate queue for just 96 and 128 bit packets, but
	// then there are potential timing concerns. May not be a real issue given how
	// 128 bit packets are used.
	// The real solution, though, will be to store just words and use a version (or write
	// a version of the boost message queue which doesn't use a separate index table, but
	// instead uses head and tail pointers like I did in my C# implementation.



	//bool Send(const void * message, const size_t messageSize, const int timeoutMilliseconds);
	//bool Receive(void * receivedMessage, size_t& messageSize, const int timeoutMilliseconds);

	//bool BlockingSend(const void * message, const size_t messageSize);
	//bool BlockingReceive(void * receivedMessage, size_t& messageSize);


};
