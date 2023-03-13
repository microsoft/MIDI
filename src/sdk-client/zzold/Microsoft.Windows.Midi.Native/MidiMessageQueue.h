// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

#pragma once

#include <Windows.h>

#include <memory>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace Windows::Devices::Midi::Internal
{
	class MidiMessageQueue
	{
	protected:
		const std::string GuidToCompactString(const GUID& guid)
		{
			const int bufferSize = 33;
			char buffer[bufferSize];

			snprintf(
				buffer,
				bufferSize,
				"%08x%04x%04x"
				"%02x%02x%02x%02x"
				"%02x%02x%02x%02x",
				guid.Data1, guid.Data2, guid.Data3,
				guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
				guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

			return std::string(buffer);
		}


		const std::string BuildQueueName(
			const GUID sessionId,
			const GUID deviceId,
			const GUID streamId)
		{
			return GuidToCompactString(sessionId) + "-" +
				GuidToCompactString(deviceId) + "-" +
				GuidToCompactString(streamId);
		}

	public:

		//virtual const bool IsEmpty() = 0;											// returns true if the queue is empty
		//virtual const bool IsFull() = 0;											// returns true if the queue is empty
		//virtual const uint64_t getCountWords() = 0;									// returns the number of words currently in the queue
		virtual const uint64_t getMaxCapacityInWords() = 0;							// returns the current max capacity
		//virtual bool Resize(uint16_t newCapacityInWords) = 0;						// resizes queue while keeping contents

		//virtual bool BeginWrite() = 0;												// for beginning a transaction / message. Locks the queue for writing.
		//virtual void EndWrite() = 0;												// end the current set and notify listeners. Unlocks queue when done.


		virtual bool WriteWords(const uint32_t* wordsBuffer, const int countWords) = 0;	// adds the number of words from the address
		virtual bool ReadWords(uint32_t* wordsBuffer, int& bufferSizeInWords, int& countWordsRead) = 0;				// updates word and count with the number actually ready

		virtual const bool PeekWord(uint32_t& word) = 0;							// gets the first word but doesn't remove it.
	};



	//// used whenever passing messages within the same process
	//// still needs thread-safety locks for local use
	//class MidiStreamLocalMessageQueue final : MidiMessageQueue
	//{
	//private:
	//	// change to a local queue
	//	//std::unique_ptr<boost::interprocess::message_queue> _queue;

	//public:
	//	virtual bool IsEmpty();												// returns true if the queue is empty
	//	virtual uint16_t getCountWords();									// returns the number of words currently in the queue
	//	virtual uint16_t getMaxCapacityInWords();							// returns the current max capacity
	//	//virtual bool Resize(uint16_t newCapacityInWords);					// resizes queue while keeping contents


	//	virtual bool BeginWrite();		// for beginning a transaction / message. Locks the queue for writing.
	//	virtual void EndWrite();			// end the current set and notify listeners. Unlocks queue when done.


	//	virtual bool WriteWord(const uint32_t& word);						// adds a single word
	//	virtual bool WriteWords(const uint32_t* words, const int count);	// adds the number of words from the address

	//	virtual bool ReadWord(uint32_t& word);								// updates word with a single word
	//	virtual bool ReadWords(uint32_t* wordsBuffer, int& count);			// updates word and count with the number actually ready

	//	virtual bool PeekWord(uint32_t& word);								// gets the first word but doesn't remove it.
	//};



	enum MidiMessageQueueType
	{
		AppMidiReadInput = 0,
		AppMidiWriteOutput = 1,

		// TODO: Other queue types? IPC needed to talk to drivers?
	};

	// used whenever passing messages across process
	// TODO: See if built-in boost IPC mechanisms are sufficient for locking and notification
	// https://www.boost.org/doc/libs/1_80_0/boost/interprocess/ipc/message_queue.hpp
	class MidiStreamCrossProcessMessageQueue final : MidiMessageQueue
	{
	private:
		std::unique_ptr<boost::interprocess::message_queue> _queue;
//		std::unique_ptr<boost::interprocess::named_mutex> _mutex;

		int DefaultPriority = 0;

		//MidiStreamCrossProcessMessageQueue() {}		// create or open through factory only

		// TODO: mutex

		bool _inWrite = false;	// true if current thread is writing

	public:

		static std::unique_ptr<MidiStreamCrossProcessMessageQueue> CreateNew(const GUID sessionId, const GUID deviceId, const GUID streamId, const MidiMessageQueueType type, const int capacityInMidiWords);
		static std::unique_ptr<MidiStreamCrossProcessMessageQueue> OpenExisting(const GUID sessionId, const GUID deviceId, const GUID streamId, const MidiMessageQueueType type);


		virtual const uint64_t getMaxCapacityInWords();							// returns the current max capacity
		//virtual bool Resize(uint16_t newCapacityInWords);					// resizes queue while keeping contents


		//virtual bool BeginWrite();			// for beginning a transaction / message. Locks the queue for writing.
		//virtual void EndWrite();			// end the current set and notify listeners. Unlocks queue when done.


		virtual bool WriteWords(const uint32_t* wordsBuffer, const int countWords);	// adds the number of words from the address
		virtual bool ReadWords(uint32_t* wordsBuffer, int& bufferSizeInWords, int& countWordsRead);			// updates word and count with the number actually ready

		virtual const bool PeekWord(uint32_t& word);							// gets the first word but doesn't remove it.

	};


	// TODO: Timestamped queue, assuming we're handling this in the service and not the client



};