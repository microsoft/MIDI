#pragma once

#include "pch.h"

#include "NetworkMidiCommandPacket.h"


enum NetworkMidiNAKReason : uint8_t
{
	NetworkMidiNAKCommandNotSupported = 0x01,
	NetworkMidiNAKCommandNotExpected = 0x02,
	NetworkMidiNAKSessionNotActive = 0x03,
	NetworkMidiNAKAuthenticationNotAccepted = 0x04,
	NetworkMidiNAKBadPingResponse = 0x05,
	NetworkMidiNAKMissingUmpPackets = 0x06,
	NetworkMidiNAKNoPendingInvitation = 0x07

};

struct NetworkMidiOutOfBandOutgoingNAKCommandPacket : public NetworkMidiOutOfBandOutgoingCommandPacket
{
public:
	networking::HostName DestinationHostName = 0;
	winrt::hstring DestinationPort;

	NetworkMidiOutOfBandOutgoingNAKCommandPacket(NetworkMidiNAKReason reason, NetworkMidiCommandPacketHeader inResponseToHeader)
	{
		Header.HeaderData.CommandCode = NetworkMidiCommandCodeNAK;
		Header.HeaderData.CommandSpecificData.AsBytes.Byte1 = reason;

		// TODO: set data bytes for the header
	}

	NetworkMidiOutOfBandOutgoingNAKCommandPacket(NetworkMidiNAKReason reason, NetworkMidiCommandPacketHeader inResponseToHeader, std::string messageText)
		: NetworkMidiOutOfBandOutgoingNAKCommandPacket(reason, inResponseToHeader)
	{
		// TODO: set bytes for the string
	}

};

struct NetworkMidiOutOfBandIncomingNAKCommandPacket : public NetworkMidiOutOfBandIncomingCommandPacket
{
public:
	NetworkMidiOutOfBandIncomingNAKCommandPacket() {}

	networking::HostName SourceHostName = 0;
	winrt::hstring  SourcePort;

};
