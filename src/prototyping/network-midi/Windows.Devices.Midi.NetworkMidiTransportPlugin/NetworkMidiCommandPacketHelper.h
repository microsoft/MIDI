#pragma once

#include "pch.h"

#include "NetworkMidiCommandCode.h"

class NetworkMidiCommandPacketHelper
{
public:
	static const size_t MaxDescriptionWidth = 13;
	static std::string GetPacketDescription(uint32_t header)
	{
		// not casting to the enum or anything here because we have to handle bad packets
		switch ((uint8_t)((header & 0xFF000000) >> 24))
		{
		case NetworkMidiCommandCodeInvitaton:
			return "Invite";
		case NetworkMidiCommandCodeInvitationWithAuthentication: 
			return "InviteA";
		case NetworkMidiCommandCodeInvitationWithUserAuthentication: 
			return "InviteUA";
		case NetworkMidiCommandCodeInvitationReplyAccepted: 
			return "InvReplyAcc";
		case NetworkMidiCommandCodeInvitationReplyPending: 
			return "InvReplyPen";
		case NetworkMidiCommandCodeInvitationReplyAuthenticationRequired: 
			return "InvReplyAReq";
		case NetworkMidiCommandCodeInvitationReplyUserAuthenticationRequired: 
			return "InvReplyAUReq";
		case NetworkMidiCommandCodePing: 
			return "Ping";
		case NetworkMidiCommandCodePingReply: 
			return "Pong";
		case NetworkMidiCommandCodeRetransmit: 
			return "Retrans";
		case NetworkMidiCommandCodeRetransmitError: 
			return "RetransErr";
		case NetworkMidiCommandCodeReport: 
			return "Report";
		case NetworkMidiCommandCodeNAK: 
			return "NAK";
		case NetworkMidiCommandCodeBye: 
			return "Bye";
		case NetworkMidiCommandCodeByeReply: 
			return "ByeReply";
		case NetworkMidiCommandCodeUmp: 
			return "UMP";
		default:
			return "(unknown)";
		}
	}
};

