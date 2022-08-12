#pragma once

#include "pch.h"

// TODO: Use Boost::uuid
struct ProtocolGuid
{
	uint32_t Part1;
	uint16_t Part2;
	uint8_t Byte1;
	uint8_t Byte2;
	uint8_t Byte3;
	uint8_t Byte4;
	uint8_t Byte5;
	uint8_t Byte6;
	uint8_t Byte7;
	uint8_t Byte8;
};

// can be easily translated to .NET version class
struct ServiceProtocolVersion
{
	int32_t Major;
	int32_t Minor;
	int32_t Build;
	int32_t Revision;
};

enum ResponseCode : uint8_t
{
	Success = 0,

	ErrorTooManyActiveConnectionsFromClient = 50,

	ErrorIncompatibleVersion = 98,
	ErrorOther = 99

};

struct RequestMessageHeader
{
	ProtocolGuid ClientId;
	ProtocolGuid ClientRequestId;

	ServiceProtocolVersion ClientVersion;
};

struct ResponseMessageHeader
{
	ProtocolGuid ClientId;
	ProtocolGuid ClientRequestId;

	ServiceProtocolVersion ServerVersion;

	ResponseCode Code;
};
