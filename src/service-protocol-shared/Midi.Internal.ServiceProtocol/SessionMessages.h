#pragma once

#include "pch.h"

#include "MessageHeaders.h"

struct CreateSessionRequestMessage
{
	RequestMessageHeader Header;

	uint32_t ProcessId;							// DWORD
	char ProcessName[MAX_PROCESS_NAME_LENGTH];	// name without the path. First N characters.
	char Name[MAX_SESSION_NAME_LENGTH];			// Session name
};

struct CreateSessionResponseMessage
{
	ResponseMessageHeader Header;
};

struct DestroySessionRequestMessage
{
	RequestMessageHeader Header;

};







