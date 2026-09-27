---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessageType
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: enum
description: The sub-id 2 byte which says what a capability inquiry message is
---

The specification sorts these into groups by number range, and the ranges matter. A device can send profile messages to a channel, a group, or a function block, but property exchange messages always go to a function block.

## Profile Configuration

| Value | Number | Description |
| ----- | ------ | ----------- |
| `ProfileInquiry` | 0x20 | Asks what profiles exist at an address |
| `ProfileInquiryReply` | 0x21 | Answers with a list of the profiles that are on and a list of the ones that are off |
| `SetProfileOn` | 0x22 | Asks a device to turn on a profile, and says how many channels to give it |
| `SetProfileOff` | 0x23 | Asks a device to turn off a profile |
| `ProfileEnabledReport` | 0x24 | Sent to everyone when a profile is turned on, for any reason |
| `ProfileDisabledReport` | 0x25 | Sent to everyone when a profile is turned off, for any reason |
| `ProfileAddedReport` | 0x26 | Sent to everyone when a device gains a profile it didn't have before |
| `ProfileRemovedReport` | 0x27 | Sent to everyone when a device loses a profile |
| `ProfileDetailsInquiry` | 0x28 | Asks about one part of how a device supports a profile |
| `ProfileDetailsInquiryReply` | 0x29 | Answers a details inquiry |
| `ProfileSpecificData` | 0x2F | Carries whatever a profile's specification says it carries |

## Property Exchange

| Value | Number | Description |
| ----- | ------ | ----------- |
| `PropertyExchangeCapabilitiesInquiry` | 0x30 | Asks how many requests the device will take at once |
| `PropertyExchangeCapabilitiesInquiryReply` | 0x31 | Answers with that number |
| `PropertyGetDataInquiry` | 0x34 | Asks for a resource |
| `PropertyGetDataInquiryReply` | 0x35 | Answers with the resource, in as many pieces as it takes |
| `PropertySetDataInquiry` | 0x36 | Sends a resource to the device |
| `PropertySetDataInquiryReply` | 0x37 | Answers a set request |
| `PropertySubscriptionInquiry` | 0x38 | Starts, updates, or ends a subscription to a resource |
| `PropertySubscriptionInquiryReply` | 0x39 | Answers a subscription message |
| `PropertyNotify` | 0x3F | Tells the other side something about a request that's in progress |

## Process Inquiry

| Value | Number | Description |
| ----- | ------ | ----------- |
| `ProcessInquiryCapabilities` | 0x40 | Asks what process inquiry features a device has |
| `ProcessInquiryCapabilitiesReply` | 0x41 | Answers with a set of flags, one for each feature |
| `MidiMessageReport` | 0x42 | Asks a device to report its current state as MIDI messages |
| `MidiMessageReportReply` | 0x43 | Says which of the requested messages will be reported |
| `MidiMessageReportEnd` | 0x44 | Marks the end of a message report |

## Management

| Value | Number | Description |
| ----- | ------ | ----------- |
| `Discovery` | 0x70 | Announces the sender and asks what's out there |
| `DiscoveryReply` | 0x71 | Answers Discovery with the device's identity and the categories it supports |
| `EndpointInquiry` | 0x72 | Asks for information about an endpoint |
| `EndpointInquiryReply` | 0x73 | Answers an endpoint inquiry |
| `Ack` | 0x7D | Acknowledges a message, usually to say a request is making progress |
| `InvalidateMuid` | 0x7E | Withdraws an identifier. It's sent to everyone and gets no reply |
| `Nak` | 0x7F | Says a request failed, with a status code and text that explains why |
