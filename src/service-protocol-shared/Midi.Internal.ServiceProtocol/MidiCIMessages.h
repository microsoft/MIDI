// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------


#pragma once

// Used internally
#include <stdint.h>

// 0x00 - 0F Reserved
// 0x10 - 1F Protocol Negotiation Messages
// 0x20 - 2F Profile Configuration Messages
// 0x30 - 3F Property Exchange Messages
// 0x40 - 6F Reserved
// 0x70 - 7F Management Messages

enum MidiCIMessageSubIds : uint8_t
{
	Discovery = 0x70,
	DiscoveryReply = 0x71,
	InvalidateMuid = 0x7E,
	Nak = 0x7F,

	InitiateProtocolNegotiation = 0x10,
	ProtocolNegotiationReply = 0x11,
	SetNewProtocol = 0x12,
	TestNewProtocol = 0x13,
	TestNewProtocolReply = 0x14,
	NewProtocolEstablished = 0x15,

	ProfileInquiry = 0x20,
	ProfileInquiryReply = 0x21,
	SetProfileOn = 0x22,
	SetProfileOff = 0x23,
	ProfileEnabledReport = 0x24,
	ProfileDisabledReport = 0x25,
	ProfileSpecificData = 0x2F,

	PropertyExchangeCapabilityInquiry = 0x30,
	PropertyExchangeCapabilitiesReply = 0x31,
	HasPropertyDataInquiry = 0x32,
	HasPropertyDataReply = 0x33,
	GetPropertyDataInquiry = 0x34,
	GetPropertyDataReply = 0x35,
	SetPropertyDataInquiry = 0x36,
	SetPropertyDataReply = 0x37,
	
	Subscription = 0x38,
	SubscriptionReply = 0x39,
	Notify = 0x3F
};

// this is just to help thinking through the messages. 
// We may not use these types directly when in production
struct MidiCIBase
{
	uint8_t SysExStartByte;		// F0
	uint8_t UniversalSysEx;		// 7E
	uint8_t DeviceId;			// 7F - entire port or 00-0F channels 1-16
	uint8_t SubId1;				// 0D
	uint8_t SubId2;				// see message types
	uint8_t CIMessageFormat;	//
	uint8_t SourceMuid[4];		// LSB first
	uint8_t DestinationMuid[4];	// LSB first

	// bytes of data go here

	uint8_t EndUniversalSysex;	// F7

};


// Message group				Scope (per bi-directional port or single channel #)
// ---------------------------- ---------------------------------------------------------------
// Discovery					Port
// Invalidate MUID				Port
// Protocol Negotiation			Port (only for those which support UMP. None for byte streams)
//		Initiate Negotiation
//		Set New Protocol
//		Test New Protocol
// Profiles						Channel or Port
//		Inquiry
//		Set Profile On
//		Set Profile Off
//		Specific Data
// Property Exchange			Channel or Port 
//		Subscription
//		Set/Get Property
//		Has Property
//		Capabilities
// NAK Response					Channel or Port
// Notify Message				Channel or Port