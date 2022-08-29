// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#include "MidiMessageQueue.h"

namespace Windows::Devices::Midi::Internal
{
	std::unique_ptr<MidiStreamCrossProcessMessageQueue> MidiStreamCrossProcessMessageQueue::CreateNew(
		const GUID sessionId, 
		const GUID deviceId, 
		const GUID streamId, 
		const MidiMessageQueueType type, 
		const int capacityInMidiWords)
	{
		assert(capacityInMidiWords > 0);

		auto q = std::make_unique<MidiStreamCrossProcessMessageQueue>();

		// Create the new queue
		q->_queue = std::make_unique<boost::interprocess::message_queue>(
			boost::interprocess::create_only,
			q->BuildQueueName(sessionId, deviceId, streamId).c_str(),
			capacityInMidiWords,
			sizeof uint32_t);

		//std::wstring mutexName(L"midi_mtx_");
		//mutexName.append(queueName);

		//// create the cross-process synchronization mechanism
		//q->_mutex = std::make_unique<boost::interprocess::named_mutex>(
		//	mutexName.c_str(),
		//	boost::interprocess::create_only
		//	);

		return q;
	}

	std::unique_ptr<MidiStreamCrossProcessMessageQueue> MidiStreamCrossProcessMessageQueue::OpenExisting(
		const GUID sessionId, 
		const GUID deviceId, 
		const GUID streamId, 
		const MidiMessageQueueType type)
	{
		auto q = std::make_unique<MidiStreamCrossProcessMessageQueue>();

		q->_queue = std::make_unique<boost::interprocess::message_queue>(
			boost::interprocess::open_only,
			q->BuildQueueName(sessionId, deviceId, streamId).c_str());

		return q;
	}

	// returns the current max capacity
	const uint64_t MidiStreamCrossProcessMessageQueue::getMaxCapacityInWords()
	{
		assert(_queue != nullptr);

		return _queue->get_max_msg();
	}


	// resizes queue while keeping contents
	//virtual bool Resize(uint16_t newCapacityInWords);					

	// for beginning a transaction / message. Locks the queue for writing.
//	bool MidiStreamCrossProcessMessageQueue::BeginWrite()
//	{
//		assert(_queue != nullptr);
//	//	assert(_mutex != nullptr);
//		assert(!_inWrite);
//
//
//		this->_inWrite = true;
//
//		// TODO: Lock queue. Pause sending updates
//
//
////		_mutex->lock();
//
//		return false;
//	}	
//	
//	// end the current set and notify listeners. Unlocks queue when done.
//	void MidiStreamCrossProcessMessageQueue::EndWrite()
//	{
//		assert(_queue != nullptr);
//	//	assert(_mutex != nullptr);
//		assert(_inWrite);
//
//		// TODO: Unlock queue. Send notifications
//
//		this->_inWrite = false;
//	}

	// adds the number of words from the address
	bool MidiStreamCrossProcessMessageQueue::WriteWords(
		const uint32_t* wordsBuffer, 
		const int bufferSizeInWords)
	{
		assert(_queue != nullptr);
		assert(wordsBuffer != nullptr);
		assert(bufferSizeInWords > 0);
		assert(_inWrite);

		_queue->send(
			wordsBuffer, 
			bufferSizeInWords*sizeof(uint32_t), 
			MidiStreamCrossProcessMessageQueue::DefaultPriority);

		return true;
	}


	// updates word and count with the number actually ready
	// buffer size needs to be at least 4 words
	// returns false if no complete messagesd to read
	bool MidiStreamCrossProcessMessageQueue::ReadWords(
		uint32_t* wordsBuffer, 
		int& bufferSizeInWords, 
		int& countWordsRead
		/* todo: timeout */)
	{
		assert(_queue != nullptr);
		assert(wordsBuffer != nullptr);
		assert(bufferSizeInWords >= 4);


		// this blocks until there's something to read or the timeout elapses.	
		//_queue->receive(wordsBuffer, count, )




		return true;
	}

	// gets the first word but doesn't remove it.
	const bool MidiStreamCrossProcessMessageQueue::PeekWord(uint32_t& word)
	{
		assert(_queue != nullptr);

		return true;
	}

};



