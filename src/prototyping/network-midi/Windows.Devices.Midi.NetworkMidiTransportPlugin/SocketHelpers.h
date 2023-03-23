#pragma once

#include "pch.h"

class SocketHelpers
{
public:
	// read UInt32 only if the data is actually there. Prevents errors
	static bool CheckedReadUInt32(const streams::DataReader& reader, uint32_t& dataOut)
	{
		if (reader.UnconsumedBufferLength() >= sizeof(uint32_t))
		{
			dataOut = reader.ReadUInt32();
			return true;
		}
		else
		{
			return false;
		}
	}

	// read bytes only if the data is actually there. Prevents errors
	static bool CheckedReadBytes(const streams::DataReader& reader, const size_t& expectedAvailableBytes, std::vector<uint8_t>& dataOut)
	{
		if (reader.UnconsumedBufferLength() >= expectedAvailableBytes)
		{
			dataOut.resize(expectedAvailableBytes);
			reader.ReadBytes(dataOut);
			return true;
		}
		else
		{
			return false;
		}
	}

	// read uint32s only if the data is actually there. Prevents errors
	static bool CheckedCopyUInt32s(streams::DataReader const& reader, const size_t& expectedAvailableWords, streams::DataWriter const& dataOut, uint16_t& wordsWritten)
	{
		wordsWritten = 0;

		if (reader.UnconsumedBufferLength() / sizeof(uint32_t) >= expectedAvailableWords)
		{
			// probably a faster way to do this maybe using the underlying buffer, but it works. 
			// We're usually talking single digits, but can be up to 349 (1400/4) 
			// (udp packet size divided by word size, minus the MIDI header)
			for (int i = 0; i < expectedAvailableWords; i++)
			{
				dataOut.WriteUInt32(reader.ReadUInt32());

				// this is redundant now. Used to be kept when we could check the destination for capacity
				wordsWritten++;
			}

			return true;
		}
		else
		{
			return false;
		}
	}


};