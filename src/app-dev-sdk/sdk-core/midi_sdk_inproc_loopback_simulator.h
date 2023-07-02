// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "pch.h"
#include <vector>
#include <queue>
#include <stdint.h>

// the simulator is for in-SDK single app testing only. It does not use the service
// or the Windows MIDI APIs. To avoid any confusion, the Ids do not show up during
// enumeration, and must be explicitly specified.
// We use this in unit tests, but also for developers to use when developing with the 
// SDK on a PC which doesn't have the rest of the MIDI Services infrastructure in place

// these are designed to look somewhat like real Ids, just to allow for folks formatting
#define MIDI_SDK_LOOPBACK_SIM1_ENDPOINT_ID L"SWD\\MIDISDK\\MIDI_Loopback1.LOOP20"
#define MIDI_SDK_LOOPBACK_SIM2_ENDPOINT_ID L"SWD\\MIDISDK\\MIDI_Loopback2.LOOP20"

namespace Microsoft::Devices::Midi2::Internal::Simulator
{
	// Change to use use Gary's circular buffer later so redundant code paths aren't needed

	struct MidiBuffer
	{
	private:
		std::queue<uint32_t> _queue = std::queue<uint32_t>();

	public:
		MidiBuffer() {};

		bool IsEmpty()
		{
			return _queue.empty();
		}

		void WriteWords(uint64_t timestamp, uint32_t* words, uint8_t wordCount)
		{
			// timestamp

			uint32_t h = (uint32_t)((timestamp & 0xFFFFFFFF00000000) >> 32);
			uint32_t l = (uint32_t)(timestamp & 0x00000000FFFFFFFF);

			_queue.push(h);
			_queue.push(l);
		}

		uint32_t PeekWord()
		{
			return _queue.front();
		}

		uint64_t ReadTimestamp()
		{
			// read the next 64 bits

			if (_queue.size() >= 2)
			{
				uint32_t h = _queue.front();
				_queue.pop();

				uint32_t l = _queue.front();
				_queue.pop();

				return (uint64_t)(((uint64_t)h << 32) | l);
			}
			else
			{
				return 0;
			}
		}

		uint32_t ReadWord()
		{
			auto f = _queue.front();
			_queue.pop();

			return f;
		}

		// TODO: Needs the incoming message callback

	};


	class MidiSdkInProcLoopbackSimulator
	{

	private:

		MidiBuffer _endpoint1OutEndpoint2In;
		MidiBuffer _endpoint1InEndpoint2Out;

		// TODO: Need buffer lock objects

	public:


		// send/receive on each side

		// signal for events

	};
}