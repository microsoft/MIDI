#pragma once

#include "pch.h"

#include "MessageHeaders.h"


struct CreateSessionRequestMessage
{
	RequestMessageHeader Header;

	uint32_t ProcessId;										// DWORD
	char ProcessName[MAX_MIDI_SESSION_PROCESS_NAME_LENGTH];	// name without the path. First N characters.
	char AppName[MAX_MIDI_SESSION_APP_NAME_LENGTH];		// Clean name for the app
	char Name[MAX_MIDI_SESSION_NAME_LENGTH];				// Session name
};

struct CreateSessionResponseMessage
{
	ResponseMessageHeader Header;

	GUID NewSessionId;
	char SessionChannelName[MAX_MIDI_SESSION_CHANNEL_NAME_LENGTH];
	SYSTEMTIME CreatedTime;			// has more than we need, but it's convenient and converts cleanly

};

struct DestroySessionRequestMessage
{
	RequestMessageHeader Header;

	GUID SessionId;

};







