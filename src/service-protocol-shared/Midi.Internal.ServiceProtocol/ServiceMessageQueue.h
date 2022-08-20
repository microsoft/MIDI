// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#pragma once

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// These are ONLY for the meta messages which get passed between client
// and the service. This is not for sending actual MIDI messages. There
// is an optimized IPC mechanism just for MIDI messages (see MidiMessageQueue)
// implementation: https://www.boost.org/doc/libs/1_80_0/boost/interprocess/ipc/message_queue.hpp

// Can also consider boost::asio::io_serivce and async_pipe if perf is not great
// But performance is supposedly much better vs old versions of the message_queue
// class


// TODO: Need to have a version which can accept multiple types of messages. Thee
// session queue is an example as it needs to handle lots of different messages
// The ping queue and the connection queue can each deal with just a single
// message type. They are also shared and so need to be kept fast and quickly
// transactional. The session queue is per session, so wouldn't slow down any
// other sessions/apps.


using namespace boost::interprocess;
using namespace boost::posix_time;

class ServiceMessageQueue
{
protected:
	std::string _queueName;
	boost::interprocess::message_queue _queue;
	const unsigned int _defaultPriority = 0;
	bool _connected = false;

public:
	// TODO: Maybe strongly type these in the implementation classes?

	//bool Send(const void * message, const size_t messageSize, const int timeoutMilliseconds);
	//bool Receive(void * receivedMessage, size_t& messageSize, const int timeoutMilliseconds);

	//bool BlockingSend(const void * message, const size_t messageSize);
	//bool BlockingReceive(void * receivedMessage, size_t& messageSize);


};



template<class Tmessage>
class SingleTypeServiceMessageQueue : ServiceMessageQueue
{
protected:

public:
	bool Send(const Tmessage& message, const int timeoutMilliseconds);
	bool Receive(Tmessage& receivedMessage, const int timeoutMilliseconds);

	bool BlockingSend(const Tmessage& message);
	bool BlockingReceive(Tmessage& receivedMessage);


};


template<class Tmessage> 
class ServiceMessageQueueServer : public SingleTypeServiceMessageQueue<Tmessage>
{
private:


public:

	bool CreateQueue(const std::string queueName, const int capacityInMessages);

	void DestroyQueue()
	{
		if (this->_connected)
		{
			message_queue::remove(this->_queueName);
			this->_connected = false;
		}
	}

};


template<class Tmessage>
class ServiceMessageQueueClient : public SingleTypeServiceMessageQueue<Tmessage>
{
public:

	bool ConnectToQueue(const std::string queueName);

	void DisconnectFromQueue()
	{
		// TODO: need to destroy the queue?

		this->_connected = false;
	}

};




