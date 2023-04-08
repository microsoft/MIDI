#pragma once

#include "pch.h"

#include "NetworkMidiCommandPacketHeader.h"


struct NetworkMidiCommandPacket
{
private:
	inline bool IsBufferSize32BitAligned(size_t bufferSizeInBytes)
	{
		return (bufferSizeInBytes % sizeof(uint32_t)) == 0;
	}

	inline size_t RoundUpBufferSizeToNext32BitAlignmentIfNeeded(size_t currentBufferSizeInBytes)
	{
		if (IsBufferSize32BitAligned(currentBufferSizeInBytes)) return currentBufferSizeInBytes;

		return (currentBufferSizeInBytes + sizeof(uint32_t) - (currentBufferSizeInBytes % sizeof(uint32_t)));
	}


	inline bool IsInternalBuffer32BitAligned()
	{
		return IsBufferSize32BitAligned(DataBuffer.size());
	}

	inline uint8_t CalculateCommandPayloadLength()
	{
		return (uint8_t)(DataBuffer.size() / sizeof(uint32_t));
	}

public:
	NetworkMidiCommandPacket() {}

	// this doesn't include the header
	std::vector<uint8_t> DataBuffer;

	NetworkMidiCommandPacketHeader Header;

	inline void Ensure32BitAlignedInternalBuffer()
	{
		if (!IsInternalBuffer32BitAligned())
		{
			DataBuffer.resize(RoundUpBufferSizeToNext32BitAlignmentIfNeeded(DataBuffer.size()));
		}

	}

	inline void SetMinimumBufferDataSizeAndAlign(uint16_t newBufferSizeInBytesExcludingHeader)
	{
		auto newSize = RoundUpBufferSizeToNext32BitAlignmentIfNeeded(newBufferSizeInBytesExcludingHeader);

		DataBuffer.resize(newSize);

		// resize zero-fills, so we don't have to
		//std::fill(DataBuffer.begin(), DataBuffer.end(), 0);

		Header.HeaderData.CommandPayloadLength = CalculateCommandPayloadLength();
	}

	inline void CopyInData(std::vector<uint8_t> source)
	{
		DataBuffer = source;

		Ensure32BitAlignedInternalBuffer();

		Header.HeaderData.CommandPayloadLength = CalculateCommandPayloadLength();
	}

	// assumes the source is null terminated
	inline void CopyInData(std::string source)
	{
		SetMinimumBufferDataSizeAndAlign((uint16_t)source.length());

		std::copy(source.begin(), source.end(), DataBuffer.begin());

		Ensure32BitAlignedInternalBuffer();

		Header.HeaderData.CommandPayloadLength = CalculateCommandPayloadLength();
	}


};


struct NetworkMidiOutOfBandOutgoingCommandPacket : public NetworkMidiCommandPacket
{
public:
	NetworkMidiOutOfBandOutgoingCommandPacket() {}

	networking::HostName DestinationHostName = 0;
	winrt::hstring DestinationPort;

};

struct NetworkMidiOutOfBandIncomingCommandPacket : public NetworkMidiCommandPacket
{
public:
	NetworkMidiOutOfBandIncomingCommandPacket() {}

	networking::HostName SourceHostName = 0;
	winrt::hstring  SourcePort;

};




