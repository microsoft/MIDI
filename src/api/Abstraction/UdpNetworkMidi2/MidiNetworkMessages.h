// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


struct MidiNetworkUdpPacket
{
    uint32_t MidiHeader;
};


struct MidiNetworkCommandPacketHeader
{
    uint8_t CommandCode;
    uint8_t CommandPayloadLength;
    uint16_t CommandSpecificData;
};



enum MidiNetworkCommandCodes
{
    ClientToHost_UmpData = 0xFF,
    ClientToHost_Invitation = 0x01,
    ClientToHost_InvitationWithAuthentication = 0x02,
    ClientToHost_InvitationWithUserAuthentication = 0x03,

    HostToClient_UmpData = 0xFF,
    HostToClient_InvitationReplyAccepted = 0x10,
    HostToClient_InvitationReplyPending = 0x11,
    HostToClient_InvitationReplyAuthenticationRequired = 0x12,
    HostToClient_InvitationReplyUserAuthenticationRequired = 0x13,

    CommonCommand_Ping = 0x20,
    CommonCommand_RetransmitRequest = 0x80,
    CommonCommand_RetransmitError = 0x81,
    CommonCommand_SessionReset = 0x82,
    CommonCommand_SessionResetReply = 0x83,
    CommonCommand_NAK = 0x8F,
    CommonCommand_Bye = 0xF0,
    CommonCommand_ByeReply = 0xF1,

};
