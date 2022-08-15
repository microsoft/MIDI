#pragma once

#include "pch.h"
#include <Windows.h>

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
	GUID ClientId;
	GUID ClientRequestId;

	ServiceProtocolVersion ClientVersion;
};

struct ResponseMessageHeader
{
	GUID ClientId;
	GUID ClientRequestId;

	ServiceProtocolVersion ServerVersion;

	ResponseCode Code;
};
