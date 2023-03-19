#pragma once

#include "pch.h"


enum NetworkMidiCommandCode : uint8_t
{
	NetworkMidiCommandCodeInvitaton = 0x01,
	NetworkMidiCommandCodeInvitationWithAuthentication = 0x02,
	NetworkMidiCommandCodeInvitationWithUserAuthentication = 0x03,
	NetworkMidiCommandCodeInvitationReplyAccepted = 0x10,
	NetworkMidiCommandCodeInvitationReplyPending = 0x11,
	NetworkMidiCommandCodeInvitationReplyAuthenticationRequired = 0x12,
	NetworkMidiCommandCodeInvitationReplyUserAuthenticationRequired = 0x13,
	NetworkMidiCommandCodePing = 0x20,
	NetworkMidiCommandCodePingReply = 0x21,
	NetworkMidiCommandCodeRetransmit = 0x80,
	NetworkMidiCommandCodeRetransmitError = 0x81,
	NetworkMidiCommandCodeReport = 0x82,
	NetworkMidiCommandCodeNAK = 0x8f,
	NetworkMidiCommandCodeBye = 0xF0,
	NetworkMidiCommandCodeByeReply = 0xF1,
	NetworkMidiCommandCodeUmp = 0xff
};