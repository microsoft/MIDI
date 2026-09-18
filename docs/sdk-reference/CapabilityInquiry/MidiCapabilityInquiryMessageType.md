---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessageType
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: enum
description: The sub-id 2 byte which says what a capability inquiry message is
---

The specification groups these into categories by range, and the ranges matter. A device may address profile messages to a channel, a group or a function block, while property exchange is only ever addressed to a function block.

## Profile Configuration

| Value | Number | Description |
| ----- | ------ | ----------- |
| `ProfileInquiry` | 0x20 | Asks what profiles exist at an address |
| `ProfileInquiryReply` | 0x21 | Answers with a list of enabled profiles and a list of disabled ones |
| `SetProfileOn` | 0x22 | Asks a device to enable a profile, and how many channels to give it |
| `SetProfileOff` | 0x23 | Asks a device to disable a profile |
| `ProfileEnabledReport` | 0x24 | Broadcast when a profile becomes enabled, for any reason |
| `ProfileDisabledReport` | 0x25 | Broadcast when a profile becomes disabled, for any reason |
| `ProfileAddedReport` | 0x26 | Broadcast when a device gains a profile it did not previously have |
| `ProfileRemovedReport` | 0x27 | Broadcast when a device loses a profile |
| `ProfileDetailsInquiry` | 0x28 | Asks about one aspect of a device's implementation of a profile |
| `ProfileDetailsInquiryReply` | 0x29 | Answers a details inquiry |
| `ProfileSpecificData` | 0x2F | Carries whatever a profile specification says it carries |

## Property Exchange

| Value | Number | Description |
| ----- | ------ | ----------- |
| `PropertyExchangeCapabilitiesInquiry` | 0x30 | Asks how many requests the device will take at once |
| `PropertyExchangeCapabilitiesInquiryReply` | 0x31 | Answers with that count |
| `PropertyGetDataInquiry` | 0x34 | Asks for a resource |
| `PropertyGetDataInquiryReply` | 0x35 | Answers with the resource, in as many chunks as it takes |
| `PropertySetDataInquiry` | 0x36 | Sends a resource to the device |
| `PropertySetDataInquiryReply` | 0x37 | Answers a set |
| `PropertySubscriptionInquiry` | 0x38 | Starts, updates or ends a subscription to a resource |
| `PropertySubscriptionInquiryReply` | 0x39 | Answers a subscription message |
| `PropertyNotify` | 0x3F | Tells the other end something about a transaction in progress |

## Process Inquiry

| Value | Number | Description |
| ----- | ------ | ----------- |
| `ProcessInquiryCapabilities` | 0x40 | Asks what process inquiry features a device has |
| `ProcessInquiryCapabilitiesReply` | 0x41 | Answers with a bitmap of them |
| `MidiMessageReport` | 0x42 | Asks a device to report its current state as MIDI messages |
| `MidiMessageReportReply` | 0x43 | Says which of the requested messages will be reported |
| `MidiMessageReportEnd` | 0x44 | Marks the end of a message report |

## Management

| Value | Number | Description |
| ----- | ------ | ----------- |
| `Discovery` | 0x70 | An initiator announcing itself and asking what is out there |
| `DiscoveryReply` | 0x71 | A responder answering, with its identity and its categories |
| `EndpointInquiry` | 0x72 | Asks for information about an endpoint |
| `EndpointInquiryReply` | 0x73 | Answers an endpoint inquiry |
| `Ack` | 0x7D | Acknowledges a message, usually to say a transaction is progressing |
| `InvalidateMuid` | 0x7E | Withdraws an identifier. Broadcast, and gets no reply |
| `Nak` | 0x7F | Says a transaction failed, with a status code and text explaining why |
