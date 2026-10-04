---
layout: sdk_reference_page
title: MidiCapabilityInquiryDeviceResponder
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: Answers capability inquiry for a virtual device
---

A [`MidiVirtualDevice`]({{ site.baseurl }}/sdk-reference/Transports/Virtual/MidiVirtualDevice) already answers endpoint discovery for you. This class does the same for capability inquiry. Give it the resources you want to publish, and the device answers Discovery, property exchange capability questions, requests for resources, profile questions, and requests for its product instance ID on its own.

The product instance ID it gives is the one in the virtual device's declared endpoint information, so endpoint discovery and capability inquiry always report the same ID. The specification allows only printable ASCII characters, up to 42 of them. If your ID doesn't fit that, the request comes to you through `MessageReceived` instead.

You get it from `MidiVirtualDevice.CapabilityInquiry`. It does nothing until you set `IsEnabled`. Answering Discovery tells others that this device supports capability inquiry, so it should only answer when that's true.

What the device says it can do comes from what you've given it, not from a setting. It says it does property exchange once it has a resource, and profile configuration once it has a profile. Saying it can do something and then not answering is worse than not saying it at all.

Identifiers belong to function blocks, not to devices. So a device with several function blocks answers Discovery once for each block, each with its own identifier. A new identifier is picked at random each time the device starts, and it must not be kept after a restart.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsEnabled` | Off until your application turns it on. While it's off, nothing is answered |
| `SupportedCategories` | What this responder will say it can do in its Discovery reply. It depends on what you've given it to answer with |
| `ReceivableMaximumSystemExclusiveSize` | The largest System Exclusive message, in bytes, this device says it can receive. It's never lower than the minimum the specification requires for profiles or property exchange, which is 512 bytes |
| `DeviceInfo` | The `DeviceInfo` resource this device publishes |
| `ChannelList` | The `ChannelList` resource this device publishes |
| `ResourceList` | The `ResourceList` resource. If you don't supply one, it's built from everything else you've set, so you don't have to describe the device twice |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetMuid(functionBlockNumber)` | The identifier for a function block. It's picked the first time you ask for it |
| `RegenerateMuid(functionBlockNumber)` | Picks a new identifier and withdraws the old one. The specification requires this when a function block moves to different groups or changes how many groups it covers |
| `SetProgramList(resourceId, programList)` | Publishes a program list. A device can publish more than one, each with its own resource id. Pass an empty id if the device has only one. Pass null to remove the list |
| `GetProgramList(resourceId)` | The program list published with that resource id, or null |
| `SetResource(resource, resourceId, jsonData)` | Publishes a resource this API has no type for, as the resource's JSON text. Use it for resources from specifications the API doesn't cover |
| `RemoveResource(resource, resourceId)` | Removes a resource published that way |
| `SetResourceSubscribable(resource, canSubscribe)` | Lets others ask to be told when this resource changes, so they don't have to keep asking. It's off for every resource until you turn it on, because a device that accepts a subscription is promising to send updates, and it should only promise that if it will |
| `IsResourceSubscribable(resource)` | Whether this resource accepts subscriptions |
| `NotifyResourceChanged(resource, resourceId)` | Sends the resource to everyone subscribed to it. Call it after you change the resource, not before. Returns how many subscribers were told |
| `SetProfiles(deviceId, enabledProfiles, disabledProfiles)` | Sets the profiles at one address. `deviceId` is `0x00` to `0x0F` for a channel, `0x7E` for a group, or `0x7F` for the whole function block |
| `SendProfileEnabledReport(functionBlockNumber, deviceId, profileId, channelCount)` | Tells everyone that a profile was turned on |
| `SendProfileDisabledReport(functionBlockNumber, deviceId, profileId, channelCount)` | Tells everyone that a profile was turned off |
| `SendProfileAddedReport(functionBlockNumber, deviceId, profileId)` | Tells everyone that this device gained a profile |
| `SendProfileRemovedReport(functionBlockNumber, deviceId, profileId)` | Tells everyone that this device lost a profile |

## Events

| Event | Description |
| ----- | ----------- |
| `MessageReceived` | Raised for every capability inquiry message this responder didn't answer itself, with all of its data. Handle it to support something the API doesn't, or to watch what the device is being asked |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/capability-inquiry-virtual-device/main_capability_inquiry_virtual_device.cpp)
* [C# Sample](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/capability-inquiry-virtual-device/Program.cs)
