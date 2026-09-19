---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessage
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: A capability inquiry message taken off the wire and decoded far enough to act on
---

Messages this API has no strong type for still arrive here with their type and addressing readable and their payload intact, so an application can handle what the API does not.

The type is divided into sections. The common fields at the top are always meaningful. Each of the property exchange, profile and acknowledgment sections is meaningful only when the matching `Has...` property says so, which is decided by what kind of message arrived rather than by the caller.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsValid` | False when the payload could not be read as a capability inquiry message |
| `MessageType` | Which message this is |
| `DeviceId` | `0x00` to `0x0F` addresses a channel, `0x7E` a group, `0x7F` the function block. Which values are permitted depends on the message: property exchange is only ever `0x7F` |
| `SourceVersion` | The MIDI-CI version the sender used to format this message |
| `SourceMuid` | Who sent it |
| `DestinationMuid` | Who it is addressed to, which may be the broadcast identifier |
| `TargetMuid` | Carried only by Invalidate MUID, naming the identifier being withdrawn |
| `OutputPathId` | Discovery only. A responder must echo this so an initiator with several outputs can tell which one the reply came back on |
| `Data` | The whole payload, from the universal System Exclusive byte onwards and without the enclosing markers. Always present, so nothing is lost for a message with no strong type |

## Property Exchange

| Property | Description |
| -------- | ----------- |
| `HasPropertyExchangeFields` | True when this is a property exchange message |
| `RequestId` | The request this message belongs to |
| `HeaderText` | The header as it arrived |
| `Header` | The header parsed. Null when the device sent something that is not a JSON object; `HeaderText` still holds what it sent so a caller can see why |
| `ChunkNumber` | One based, as the specification numbers them. Chunk zero does not exist, and a device sends zero here to say the data it was sending is no longer good |
| `ChunkCount` | How many chunks the whole message takes. Zero when the sender does not know in advance |
| `Body` | This chunk's slice of the property data, which is only the whole resource when the message is the single chunk of a one chunk reply |

## Profile Configuration

| Property | Description |
| -------- | ----------- |
| `HasProfileFields` | True when this is a profile message |
| `ProfileId` | The profile the message is about. Null for Profile Inquiry and for the reply to it, which deal in whole lists rather than in any one profile |
| `ProfileChannelCount` | Channels asked for by Set Profile On, or taken by an enabled or disabled report. Zero when the message addresses a group or a function block, and zero when it came from a device speaking the first message version |
| `HasProfileInquiryTarget` | True when the message carries an inquiry target |
| `ProfileInquiryTarget` | Below `0x40` the meaning is common to every profile; from `0x40` up the profile specification defines it |
| `ProfileData` | The payload of a details reply, or of Profile Specific Data |
| `EnabledProfiles` | From a reply to Profile Inquiry. Profiles the responder supports and which are active now |
| `DisabledProfiles` | From the same reply. Profiles the responder can support but which are not active |

## Acknowledgment

| Property | Description |
| -------- | ----------- |
| `HasAcknowledgmentFields` | True for ACK and NAK. A device speaking the first message version sends neither set of fields, so this is false even though the message itself is well formed |
| `OriginalMessageType` | Which message this answers, so a reply can be matched to the transaction that caused it |
| `StatusCode` | Why the transaction failed |
| `StatusData` | Extra information for status codes that need it |
| `StatusDetails` | Five bytes whose meaning depends on the branch: the profile identifier for a profile message, and the request id followed by the chunk number for property exchange |
| `StatusMessage` | Text the device sent to explain the failure. The specification says to show it to the person using the application |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `FromSystemExclusiveData(data)` | Decodes from the assembled System Exclusive payload, excluding the `0xF0` and `0xF7` markers |
| `FromUmpMessages(messages)` | Decodes straight from the packets which carried it. The messages must be the complete System Exclusive transfer, in order |
| `IsCapabilityInquiryData(data)` | True when the payload is a capability inquiry message at all, without decoding it |
