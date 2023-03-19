#pragma once

#include "pch.h"

class SocketHelpers
{
public:
	static bool CheckedSocketReadUInt32(const streams::DataReader& reader, uint32_t& dataOut)
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

	static bool CheckedSocketReadBytes(const streams::DataReader& reader, const size_t& expectedAvailableBytes, std::vector<uint8_t>& dataOut)
	{
		if (reader.UnconsumedBufferLength() >= expectedAvailableBytes)
		{
			dataOut.reserve(expectedAvailableBytes);
			reader.ReadBytes(dataOut);
			return true;
		}
		else
		{
			return false;
		}
	}


	static bool CheckedSocketReadUInt32s(const streams::DataReader& reader, const size_t& expectedAvailableWords, std::vector<uint32_t>& dataOut)
	{
		if (reader.UnconsumedBufferLength() / sizeof(uint32_t) >= expectedAvailableWords)
		{
			dataOut.resize(expectedAvailableWords);

			// probably a faster way to do this maybe using the underlying buffer, but it works. 
			// We're usually talking single digits, but can be up to 349 (1400/4) 
			// (udp packet size divided by word size, minus the MIDI header)
			for (int i = 0; i < expectedAvailableWords; i++)
			{
				dataOut[i] = reader.ReadUInt32();
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	static bool CheckedSocketReadUInt32s(const streams::DataReader& reader, const size_t& expectedAvailableWords, streams::DataWriter& dataOut, uint16_t& wordsWritten)
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