---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessage
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A capability inquiry message that arrived, decoded far enough to act on
---

You can read any capability inquiry message with this class, even one this API has no special type for. You can still read its type and who it's addressed to, and its data arrives complete, so your application can handle messages the API doesn't.

The properties come in groups. The common ones below always have values. The property exchange, profile, acknowledgment, and endpoint inquiry groups have values only when the matching `Has...` property is true, and that depends on what kind of message arrived.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsValid` | False when the data couldn't be read as a capability inquiry message |
| `MessageType` | Which message this is |
| `DeviceId` | Who the message is for on the device: `0x00` to `0x0F` is a channel, `0x7E` is a group, and `0x7F` is the whole function block. Which values are allowed depends on the message. Property exchange always uses `0x7F` |
| `SourceVersion` | The MIDI-CI version the sender used to format this message |
| `SourceMuid` | The identifier of the sender |
| `DestinationMuid` | The identifier it's addressed to. This may be the broadcast identifier, which means everyone |
| `TargetMuid` | Only in an Invalidate MUID message. It's the identifier being withdrawn |
| `OutputPathId` | Only in Discovery messages. A device that answers must send this back, so the one that asked can tell which of its outputs the reply came from |
| `Data` | All the message data, starting with the Universal System Exclusive byte, without the start and end markers. It's always filled in, so nothing is lost, even for a message this API has no special type for |

## Property Exchange

| Property | Description |
| -------- | ----------- |
| `HasPropertyExchangeFields` | True when this is a property exchange message |
| `RequestId` | The request this message belongs to |
| `HeaderText` | The header as it arrived |
| `Header` | The header, read as JSON. It's null when the device sent something that isn't a JSON object. `HeaderText` still holds what it sent, so you can see what went wrong |
| `ChunkNumber` | Which piece of the reply this is, counting from 1, the way the specification counts. There's no chunk 0: a device sends 0 here to say the data it was sending is no longer good |
| `ChunkCount` | How many pieces the whole message takes. 0 when the sender doesn't know ahead of time |
| `Body` | This piece of the property data. It's the whole resource only when the reply fits in one piece |

## Profile Configuration

| Property | Description |
| -------- | ----------- |
| `HasProfileFields` | True when this is a profile message |
| `ProfileId` | The profile the message is about. Null for Profile Inquiry and its reply, which are about whole lists of profiles, not one profile |
| `ProfileChannelCount` | How many channels a Set Profile On asks for, or an enabled or disabled report covers. 0 when the message is for a group or a whole function block, and 0 when it came from a device that uses the first message version |
| `HasProfileInquiryTarget` | True when the message carries an inquiry target |
| `ProfileInquiryTarget` | Below `0x40`, the value means the same thing for every profile. From `0x40` up, each profile's own specification says what it means |
| `ProfileData` | The data in a Profile Details reply or a Profile Specific Data message |
| `EnabledProfiles` | From a Profile Inquiry reply: the profiles the device supports that are turned on now |
| `DisabledProfiles` | From the same reply: the profiles the device supports that are turned off |

## Acknowledgment

| Property | Description |
| -------- | ----------- |
| `HasAcknowledgmentFields` | True for ACK and NAK messages. A device that uses the first message version doesn't send these fields, so this is false for it even though the message is fine |
| `OriginalMessageType` | Which message this answers, so you can match the reply to what caused it |
| `StatusCode` | The status code. For a NAK, it says why the request failed |
| `StatusData` | Extra information for status codes that need it |
| `StatusDetails` | Five bytes whose meaning depends on the kind of message this answers. For a profile message, it's the profile id. For property exchange, it's the request id followed by the chunk number |
| `StatusMessage` | Text the device sent to explain. The specification says to show it to the person using your application |

## Endpoint Inquiry

| Property | Description |
| -------- | ----------- |
| `HasEndpointFields` | True for an endpoint inquiry and the reply to it |
| `EndpointStatus` | Which piece of endpoint information is asked for or given. `0x00` is the product instance ID, and it's the only one defined so far |
| `EndpointInformation` | What a reply carries. For status `0x00`, it's the product instance ID as ASCII text |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromSystemExclusiveData(data)` | Decodes a message from its System Exclusive data, put back together, without the `0xF0` and `0xF7` markers |
| `FromUmpMessages(messages)` | Decodes a message straight from the packets that carried it. Pass every packet of the System Exclusive message, in order |
| `IsCapabilityInquiryData(data)` | Returns true if the data is a capability inquiry message, without decoding it |
