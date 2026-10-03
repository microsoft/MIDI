---
layout: sdk_reference_page
title: MidiCapabilityInquiryMessageBuilder
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: Builds capability inquiry messages as the UMP packets that carry them
---

You can pass what these functions return straight to an endpoint connection. If you give a function something that isn't allowed inside a System Exclusive message, it returns an empty list instead of a half-built message.

Nothing here sends anything, and nothing here remembers anything between calls. Your code keeps its own identifier and its own request numbers, and decides what to ask for. Most applications should use [`MidiCapabilityInquirySession`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquirySession) instead, which does all of that for you. This class is for applications that need something the session doesn't cover.

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `MinimumReceivableSystemExclusiveSize` | Every device that supports profiles or property exchange must be able to receive at least this much System Exclusive data, in bytes. So it's the safe size to assume for a device that hasn't said otherwise |

## Static Methods: Management

| Static Method | Description |
| ------------- | ----------- |
| `BuildDiscovery(timestamp, group, sourceMuid, identity, supportedCategories, receivableMaximumSystemExclusiveSize, outputPathId)` | Announces the sender to every device. It's always sent to the broadcast identifier, because the sender doesn't know yet what's out there. The output path id says which of the sender's outputs this message went out on. A device that answers sends it back, so an application with several outputs can tell which one reached the device |
| `BuildDiscoveryReply(timestamp, group, sourceMuid, destinationMuid, identity, supportedCategories, receivableMaximumSystemExclusiveSize, outputPathId, functionBlockNumber)` | Answers Discovery. A device must answer, even when it supports no categories at all |
| `BuildInvalidateMuid(timestamp, group, sourceMuid, muidToInvalidate)` | Withdraws an identifier, which is what a device does when it has to change the one it was using. It's sent to everyone and gets no reply |
| `BuildEndpointInquiry(timestamp, group, sourceMuid, destinationMuid, status)` | Asks for one piece of information about the UMP endpoint that a function block belongs to. The status says which. `0x00` asks for the product instance ID, and it's the only status defined so far |
| `BuildEndpointReply(timestamp, group, sourceMuid, destinationMuid, status, information)` | Answers an endpoint inquiry with the same status it asked with. For status `0x00`, the information is the product instance ID the endpoint also gives in endpoint discovery. The specification allows only printable ASCII characters there, up to 42 of them, so an ID that breaks those rules gets an empty list back |
| `BuildAck(timestamp, group, deviceId, sourceMuid, destinationMuid, originalMessageType, statusCode, statusData, statusDetails, statusMessage)` | Acknowledges a message. The status message is shown to the person using the other application, so write it with care. It's sent as 7-bit text, and anything outside that range is refused rather than garbled |
| `BuildNak(timestamp, group, deviceId, sourceMuid, destinationMuid, originalMessageType, statusCode, statusData, statusDetails, statusMessage)` | Says a request failed, and why |

## Static Methods: Property Exchange

| Static Method | Description |
| ------------- | ----------- |
| `BuildPropertyExchangeCapabilitiesInquiry(timestamp, group, sourceMuid, destinationMuid, maximumSimultaneousRequests, messageVersion)` | Asks how many requests each end is willing to have waiting at once. Send this before asking for anything else: a device that doesn't answer it doesn't do property exchange. `messageVersion` picks the layout. This message gained two bytes in MIDI-CI 1.2, and a MIDI-CI 1.1 device expects the shorter one, so pass the version the device gave in its own Discovery reply (`MidiCapabilityInquiryResponder.MessageVersion`), not the newest version. A value below `2` writes the MIDI-CI 1.1 form, and `2` or higher writes the 1.2 form |
| `BuildPropertyExchangeCapabilitiesReply(timestamp, group, sourceMuid, destinationMuid, maximumSimultaneousRequests, messageVersion)` | The answer to the same. Pass the version the other device declared, for the same reason |
| `BuildPropertyGetDataInquiry(timestamp, group, sourceMuid, destinationMuid, requestId, header)` | Asks for a resource. The header names it and holds any options, for example the offset and limit that page through a long list |
| `BuildPropertyMessage(timestamp, group, messageType, sourceMuid, destinationMuid, requestId, header, body, destinationMaximumSystemExclusiveSize)` | Builds any property exchange message. It's split into as many pieces (chunks) as the destination's size limit needs, and each piece into as many packets as it needs. The whole transfer comes back as one list, in the order it must be sent. The header goes only in the first piece, as the specification requires, so don't try to build the pieces yourself by calling this once for each one |
| `GetPropertyChunkCount(header, bodyByteCount, destinationMaximumSystemExclusiveSize)` | How many pieces the same call would make, without making them. Use it to decide whether a transfer is worth starting, and to show progress once it has started |

## Static Methods: Profiles

Profile messages can be sent to one channel with `0x00` to `0x0F`, a whole group with `0x7E`, or a whole function block with `0x7F`. Which of those a device answers is up to the device.

| Static Method | Description |
| ------------- | ----------- |
| `BuildProfileInquiry(timestamp, group, deviceId, sourceMuid, destinationMuid)` | Asks what profiles exist at an address |
| `BuildProfileInquiryReply(timestamp, group, deviceId, sourceMuid, destinationMuid, enabledProfiles, disabledProfiles)` | Answers with both lists. A device with no profiles at the address still answers, with both lists empty |
| `BuildSetProfileOn(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, channelCount)` | Asks a device to turn on a profile. Send a channel count of zero when the message is for a group or a function block, because its size is already set |
| `BuildSetProfileOff(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId)` | Asks a device to turn off a profile |
| `BuildProfileEnabledReport(timestamp, group, deviceId, sourceMuid, profileId, channelCount)` | Tells everyone a profile was turned on, so it has no destination |
| `BuildProfileDisabledReport(timestamp, group, deviceId, sourceMuid, profileId, channelCount)` | Tells everyone a profile was turned off |
| `BuildProfileAddedReport(timestamp, group, deviceId, sourceMuid, profileId)` | Tells everyone a profile was added |
| `BuildProfileRemovedReport(timestamp, group, deviceId, sourceMuid, profileId)` | Tells everyone a profile was removed |
| `BuildProfileDetailsInquiry(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, inquiryTarget)` | Asks what a device's version of a profile can do, which can be worth knowing before you turn it on. An inquiry target below `0x40` means the same thing for every profile. From `0x40` up, the profile's own specification says what it means |
| `BuildProfileDetailsReply(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, inquiryTarget, inquiryTargetData)` | Answers a details inquiry |
| `BuildProfileSpecificData(timestamp, group, deviceId, sourceMuid, destinationMuid, profileId, data)` | Carries whatever a profile's specification says it carries. This API doesn't read it |

## Static Methods: Raw

| Static Method | Description |
| ------------- | ----------- |
| `BuildFromSystemExclusiveData(timestamp, group, data)` | Packs a capability inquiry message that this API has no builder for. The bytes start at the Universal System Exclusive byte and leave out the start and end markers. That's the same shape `MidiCapabilityInquiryMessage.FromSystemExclusiveData` reads and its `Data` property returns, so you can take a message apart, change it, and send it on |
