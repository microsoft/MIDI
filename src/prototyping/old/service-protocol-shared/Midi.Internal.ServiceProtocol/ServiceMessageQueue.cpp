// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------
#include "pch.h"
#include "ServiceMessageQueue.h"




template <class Tmessage>
bool SingleTypeServiceMessageQueue<Tmessage>::Send(const Tmessage& message, const int timeoutMilliseconds)
{
	if (!this->_connected) return false;

	ptime timeout = microsec_clock::universal_time() + milliseconds(timeoutMilliseconds);

	return this->_queue.timed_send(&message, sizeof(message), _defaultPriority, timeout);
}

template <class Tmessage>
bool SingleTypeServiceMessageQueue<Tmessage>::BlockingSend(const Tmessage& message)
{
	if (!this->_connected) return false;

	this->_queue.send(&message, sizeof(message), _defaultPriority);

	return true;
}

template <class Tmessage>
bool SingleTypeServiceMessageQueue<Tmessage>::Receive(Tmessage& receivedMessage, const int timeoutMilliseconds)
{
	if (!this->_connected) return false;

	ptime timeout = microsec_clock::universal_time() + milliseconds(timeoutMilliseconds);
	size_t receiveSize = 0;
	unsigned int priority = _defaultPriority;

	return this->_queue.timed_receive(&receivedMessage, sizeof(receivedMessage), receiveSize, priority, timeout);
}

template <class Tmessage>
bool SingleTypeServiceMessageQueue<Tmessage>::BlockingReceive(Tmessage& receivedMessage)
{
	if (!this->_connected) return false;

	size_t receiveSize = 0;
	unsigned int priority = _defaultPriority;

	this->_queue.receive(&receivedMessage, sizeof(receivedMessage), receiveSize, priority);

	return receiveSize != 0;
}













template <class Tmessage>
bool ServiceMessageQueueServer<Tmessage>::CreateQueue(const std::string queueName, const int capacityInMessages)
{
	BOOST_TRY
	{
		//	permissions perms;
		//	perms.set_default();

		size_t maxMessageSize = sizeof(Tmessage);

		this->_queueName = queueName;
		message_queue mq(boost::interprocess::create_only, queueName, capacityInMessages, maxMessageSize); // , perms);

		this->_queue = mq;
		this->_connected = true;
	}
	BOOST_CATCH(interprocess_exception& ex)
	{
		//	std::cout << ex.what() << std:endl;
		return false;
	} BOOST_CATCH_END

	return true;
}




template<class Tmessage>
bool ServiceMessageQueueClient<Tmessage>::ConnectToQueue(const std::string queueName)
{
	BOOST_TRY
	{
		//	permissions perms;
		//	perms.set_default();

		size_t maxMessageSize = sizeof(Tmessage);

		this->_queueName = queueName;
		message_queue mq(boost::interprocess::open_only, queueName);

		this->_queue = mq;
		this->_connected = true;
	}
		BOOST_CATCH(interprocess_exception& ex)
	{
		//	std::cout << ex.what() << std:endl;
		return false;
	} BOOST_CATCH_END

	return true;

}