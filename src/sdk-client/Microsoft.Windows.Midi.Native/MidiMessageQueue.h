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

	public:
		virtual bool IsEmpty() = 0;												// returns true if the queue is empty
		virtual uint16_t getCountWords() = 0;									// returns the number of words currently in the queue
		virtual uint16_t getMaxCapacityInWords() = 0;							// returns the current max capacity
		//virtual bool Resize(uint16_t newCapacityInWords) = 0;					// resizes queue while keeping contents

		virtual bool BeginWrite() = 0;											// for beginning a transaction / message. Locks the queue for writing.
		virtual void EndWrite() = 0;											// end the current set and notify listeners. Unlocks queue when done.


		virtual bool WriteWord(const uint32_t& word) = 0;						// adds a single word
		virtual bool WriteWords(const uint32_t* words, const int count) = 0;	// adds the number of words from the address

		virtual bool ReadWord(uint32_t& word) = 0;								// updates word with a single word
		virtual bool ReadWords(uint32_t* wordsBuffer, int& count) = 0;			// updates word and count with the number actually ready

		virtual bool PeekWord(uint32_t& word) = 0;								// gets the first word but doesn't remove it.
	};



	// used whenever passing messages within the same process
	// still needs thread-safety locks for local use
	class MidiStreamLocalMessageQueue final : MidiMessageQueue
	{
	private:
		// change to a local queue
		//std::unique_ptr<boost::interprocess::message_queue> _queue;

	public:
		virtual bool IsEmpty();												// returns true if the queue is empty
		virtual uint16_t getCountWords();									// returns the number of words currently in the queue
		virtual uint16_t getMaxCapacityInWords();							// returns the current max capacity
		//virtual bool Resize(uint16_t newCapacityInWords);					// resizes queue while keeping contents


		virtual bool BeginWrite();		// for beginning a transaction / message. Locks the queue for writing.
		virtual void EndWrite();			// end the current set and notify listeners. Unlocks queue when done.


		virtual bool WriteWord(const uint32_t& word);						// adds a single word
		virtual bool WriteWords(const uint32_t* words, const int count);	// adds the number of words from the address

		virtual bool ReadWord(uint32_t& word);								// updates word with a single word
		virtual bool ReadWords(uint32_t* wordsBuffer, int& count);			// updates word and count with the number actually ready

		virtual bool PeekWord(uint32_t& word);								// gets the first word but doesn't remove it.
	};



	// used whenever passing messages across process
	// TODO: See if built-in boost IPC mechanisms are sufficient for locking and notification
	// https://www.boost.org/doc/libs/1_80_0/boost/interprocess/ipc/message_queue.hpp
	class MidiStreamCrossProcessMessageQueue final : MidiMessageQueue
	{
	private:
		std::unique_ptr<boost::interprocess::message_queue> _queue;

	public:
		MidiStreamCrossProcessMessageQueue(const int capacityInMidiWords, const char* name);


		virtual bool IsEmpty();												// returns true if the queue is empty
		virtual uint16_t getCountWords();									// returns the number of words currently in the queue
		virtual uint16_t getMaxCapacityInWords();							// returns the current max capacity
		//virtual bool Resize(uint16_t newCapacityInWords);					// resizes queue while keeping contents


		virtual bool BeginWrite();		// for beginning a transaction / message. Locks the queue for writing.
		virtual void EndWrite();			// end the current set and notify listeners. Unlocks queue when done.


		virtual bool WriteWord(const uint32_t& word);						// adds a single word
		virtual bool WriteWords(const uint32_t* words, const int count);	// adds the number of words from the address

		virtual bool ReadWord(uint32_t& word);						// updates word with a single word
		virtual bool ReadWords(uint32_t* wordsBuffer, int& count);			// updates word and count with the number actually ready

		virtual bool PeekWord(uint32_t& word);							// gets the first word but doesn't remove it.

	};


	// TODO: Timestamped queue, assuming we're handling this in the service and not the client



};