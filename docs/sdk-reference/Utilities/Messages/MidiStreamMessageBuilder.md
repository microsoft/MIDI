---
layout: sdk_reference_page
title: MidiStreamMessageBuilder
namespace: Windows.Devices.Midi2.Utilities.Messages
type: runtimeclass
description: Helper class to construct and parse MIDI 2.0 stream messages
---

Stream messages are how a MIDI 2.0 endpoint describes itself and agrees on a protocol. They carry its name, its identity, its function blocks, and whether it will use the MIDI 1.0 or MIDI 2.0 protocol. The MIDI service has this conversation with devices for you, so most applications never need these functions.

The main exception is an application that creates a virtual device. It answers the questions itself, so it uses these functions to build its answers. For example, it calls `BuildStreamConfigurationNotificationMessage` to answer a stream configuration request.

The messages are described in the MIDI Association's Universal MIDI Packet (UMP) and MIDI 2.0 Protocol specification. In the parameter names, "JR" means jitter reduction.

## Static Functions: Building Messages

Every function takes a `timestamp` first. Pass `0` or `MidiClock.TimestampConstantSendImmediately` to send the message right away.

| Function | Description |
| -------- | ----------- |
| `BuildEndpointDiscoveryMessage(timestamp, umpVersionMajor, umpVersionMinor, request)` | Asks an endpoint to describe itself. Most applications pass `1` and `1` for UMP version 1.1. `request` is a `MidiEndpointDiscoveryRequests` value that says which answers you want |
| `BuildEndpointInfoNotificationMessage(timestamp, umpVersionMajor, umpVersionMinor, hasStaticFunctionBlocks, numberOfFunctionBlocks, supportsMidi20Protocol, supportsMidi10Protocol, supportsReceivingJitterReductionTimestamps, supportsSendingJitterReductionTimestamps)` | Answers with the basic facts about an endpoint: the UMP version it follows, how many function blocks it has and whether they can change, which protocols it supports, and whether it sends or receives jitter reduction timestamps |
| `BuildDeviceIdentityNotificationMessage(timestamp, deviceManufacturerSysExIdByte1, deviceManufacturerSysExIdByte2, deviceManufacturerSysExIdByte3, deviceFamilyLsb, deviceFamilyMsb, deviceFamilyModelNumberLsb, deviceFamilyModelNumberMsb, softwareRevisionLevelByte1, softwareRevisionLevelByte2, softwareRevisionLevelByte3, softwareRevisionLevelByte4)` | Answers with the device's identity: the manufacturer's System Exclusive id, the device family and model, and the software version |
| `BuildEndpointNameNotificationMessages(timestamp, name)` | Answers with the endpoint's name. A long name needs more than one message, so this returns a list. The name is sent as UTF-8, and anything past the 98-byte limit is dropped |
| `BuildProductInstanceIdNotificationMessages(timestamp, productInstanceId)` | Answers with the product instance id, which is usually a serial number. Returns a list. Anything past the 42-byte limit is dropped |
| `BuildStreamConfigurationRequestMessage(timestamp, protocol, expectToReceiveJRTimestamps, requestToSendJRTimestamps)` | Asks an endpoint to switch protocols. Pass `0x01` for MIDI 1.0 or `0x02` for MIDI 2.0. The two flags ask for jitter reduction timestamps in each direction |
| `BuildStreamConfigurationNotificationMessage(timestamp, protocol, confirmationWillReceiveJRTimestamps, confirmationSendJRTimestamps)` | Answers a stream configuration request by saying which protocol and timestamp settings are now in use |
| `BuildFunctionBlockDiscoveryMessage(timestamp, functionBlockNumber, requestFlags)` | Asks about one function block, or pass `0xFF` to ask about all of them. `requestFlags` is a `MidiFunctionBlockDiscoveryRequests` value that says which answers you want |
| `BuildFunctionBlockInfoNotificationMessage(timestamp, active, functionBlockNumber, uiHint, midi10, direction, firstGroup, numberOfGroups, midiCIVersionFormat, maxNumberSysEx8Streams)` | Describes one function block: whether it's active, which direction it goes, which groups it covers, how it should be shown to people, whether it stands for a MIDI 1.0 connection, and what MIDI-CI and System Exclusive 8 support it has. `firstGroup` is a group index from 0 to 15 |
| `BuildFunctionBlockNameNotificationMessages(timestamp, functionBlockNumber, name)` | Answers with a function block's name. Returns a list. The name is sent as UTF-8, and anything past the 91-byte limit is dropped |

## Static Functions: Reading Messages

| Function | Description |
| -------- | ----------- |
| `ParseEndpointNameNotificationMessages(messages)` | Puts an endpoint name back together from the messages that carried it. Pass only the messages for one name, in the order they arrived. The text is read as UTF-8 |
| `ParseProductInstanceIdNotificationMessages(messages)` | The same, for a product instance id |
| `ParseFunctionBlockNameNotificationMessages(messages)` | The same, for a function block name |
| `ParseDeviceIdentityNotificationMessage(message)` | Reads a Device Identity Notification into a [`MidiDeclaredDeviceIdentity`]({{ site.baseurl }}/sdk-reference/Enumeration/MidiDeclaredDeviceIdentity). Returns null when the message is something else, so you can pass it any stream message and check the result |
