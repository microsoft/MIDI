---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessageBuilder
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: Builds capability inquiry messages as the UMP packets which carry them
---

What comes out can be handed straight to an endpoint connection. Every method returns an empty list rather than a half formed message when it is given something that cannot legally travel inside a System Exclusive message.

Nothing here sends anything, and nothing here keeps state. A caller owns its own identifier, its own request numbering and its own decisions about what to ask for. Most applications will want [`MidiCapabilityInquirySession`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquirySession) instead, which owns all of that; this is for an application doing something the session does not cover.

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `MinimumReceivableSystemExclusiveSize` | Every device which implements profiles or property exchange has to be able to receive at least this much System Exclusive data, so it is the safe assumption about a device which has not said otherwise |

## Static Methods: Management

| Static Method | Description |
| ------------- | ----------- |
| `BuildDiscovery(timestamp, group, sourceMuid, identity, supportedCategories, receivableMaximumSystemExclusiveSize, outputPathId)` | An initiator announcing itself. Always goes to the broadcast identifier, because the initiator does not yet know what is out there to address. The output path id names which of the initiator's own outputs this went out of; a responder echoes it, which is how an application with several outputs can tell which one reached the device |
| `BuildDiscoveryReply(timestamp, group, sourceMuid, destinationMuid, identity, supportedCategories, receivableMaximumSystemExclusiveSize, outputPathId, functionBlockNumber)` | A responder must answer Discovery even when it supports no categories at all |
| `BuildInvalidateMuid(timestamp, group, sourceMuid, muidToInvalidate)` | Withdraws an identifier, which is what a device does when it has to change the one it was using. Broadcast, and gets no reply |
| `BuildAck(timestamp, group, deviceId, sourceMuid, destinationMuid, originalMessageType, statusCode, statusData, statusDetails, statusMessage)` | Acknowledges a message. The status message is shown to the person using the application, so it is worth writing. It is seven bit text on the wire; anything outside that is refused rather than mangled |
| `BuildNak(timestamp, group, deviceId, sourceMuid, destinationMuid, originalMessageType, statusCode, statusData, statusDetails, statusMessage)` | Says a transaction failed, and why |

## Static Methods: Property Exchange

| Static Method | Description |
| ------------- | ----------- |
| `BuildPropertyExchangeCapabilitiesInquiry(timestamp, group, sourceMuid, destinationMuid, maximumSimultaneousRequests)` | How many requests each end is willing to have outstanding at once |
| `BuildPropertyExchangeCapabilitiesReply(timestamp, group, sourceMuid, destinationMuid, maximumSimultaneousRequests)` | The answer to the same |
| `BuildPropertyGetDataInquiry(timestamp, group, sourceMuid, destinationMuid, requestId, header)` | Asks for a resource. The header names it and carries any options, for example the offset and limit that page through a long list |
| `BuildPropertyMessage(timestamp, group, messageType, sourceMuid, destinationMuid, requestId, header, body, destinationMaximumSystemExclusiveSize)` | Any property exchange message, split into as many chunks as the destination's declared size needs and each chunk into as many packets as it needs. The whole transfer comes back as one list, in the order it must be sent. The header travels on the first chunk only, which is what the specification requires, so a caller must not try to chunk by calling this once per chunk |
| `GetPropertyChunkCount(header, bodyByteCount, destinationMaximumSystemExclusiveSize)` | How many chunks the same call will produce, without producing them. Lets a caller decide whether a transfer is worth starting, and show progress once it has |

## Static Methods: Profiles

Profile messages may address one channel with `0x00` to `0x0F`, a whole group with `0x7E`, or a whole function block with `0x7F`. Which of those a device will answer is its own business.

| Static Method | Description |
| ------------- | ----------- |
| `BuildProfileInquiry(timestamp, group, deviceId, sourceMuid, destinationMuid)` | Asks what profiles exist at an address |
| `BuildProfileInquiryReply(timestamp, group, deviceId, sourceMuid, destinationMuid, enabledProfiles, disabledProfiles)` | Answers with both lists. A device with no profiles at the address still replies, with both lists empty |
| `BuildSetProfileOn(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, channelCount)` | Asks a device to enable a profile. Send a channel count of zero when the message addresses a group or a function block, whose width is already decided |
| `BuildSetProfileOff(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId)` | Asks a device to disable a profile |
| `BuildProfileEnabledReport(timestamp, group, deviceId, sourceMuid, profileId, channelCount)` | Broadcast, so it takes no destination |
| `BuildProfileDisabledReport(timestamp, group, deviceId, sourceMuid, profileId, channelCount)` | Broadcast |
| `BuildProfileAddedReport(timestamp, group, deviceId, sourceMuid, profileId)` | Broadcast |
| `BuildProfileRemovedReport(timestamp, group, deviceId, sourceMuid, profileId)` | Broadcast |
| `BuildProfileDetailsInquiry(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, inquiryTarget)` | Asks what a device's implementation of a profile can do, which may be worth knowing before enabling it. An inquiry target below `0x40` means the same thing for every profile; from `0x40` up the profile's own specification defines it |
| `BuildProfileDetailsReply(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, inquiryTarget, inquiryTargetData)` | Answers a details inquiry |
| `BuildProfileSpecificData(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, data)` | Carries whatever a profile specification says it carries. This API does not interpret it |

## Static Methods: Raw

| Static Method | Description |
| ------------- | ----------- |
| `BuildFromSystemExclusiveData(timestamp, group, data)` | Packs a capability inquiry payload this API has no builder for. The bytes start at the universal System Exclusive byte and exclude the enclosing markers, which is the same shape `MidiCapabilityInquiryMessage.FromSystemExclusiveData` reads and the same shape its `Data` property returns, so a message can be taken apart, altered and sent on |
