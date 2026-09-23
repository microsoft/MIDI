---
layout: sdk_reference_page
title: MidiCapabilityInquiryDeviceResponder
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
description: Answers capability inquiry on a virtual device's behalf
---

A [`MidiVirtualDevice`]({{ site.baseurl }}/sdk-reference/Transports/Virtual/MidiVirtualDevice) already answers endpoint discovery without the application thinking about it. This gives it the same treatment for capability inquiry: an application hands over the resources it wants to publish and the device answers Discovery, property exchange capabilities, get requests and profile inquiries on its own.

It is reached through `MidiVirtualDevice.CapabilityInquiry` and does nothing until `IsEnabled` is set. Answering Discovery is a device saying it implements capability inquiry, and it should only say that when it means it.

What the device declares it can do follows from what it has been given rather than from a flag. Property exchange appears once there is a resource, and profile configuration appears once there is a profile. Declaring a category and then answering nothing is worse than not declaring it.

Identifiers belong to function blocks rather than to devices, so a device with several function blocks answers Discovery once per block, each with its own identifier. Each is drawn fresh at startup and must not survive a restart.

## Properties

| Property | Description |
| -------- | ----------- |
| `IsEnabled` | Off until an application sets it. Nothing is answered while it is off |
| `SupportedCategories` | What this responder will declare in a Discovery reply, which follows from what it has been given to answer with |
| `ReceivableMaximumSystemExclusiveSize` | The largest System Exclusive message this device declares it can receive. Never lower than the minimum the specification requires of anything doing profiles or property exchange |
| `DeviceInfo` | The `DeviceInfo` resource this device publishes |
| `ChannelList` | The `ChannelList` resource this device publishes |
| `ResourceList` | The `ResourceList` resource. Built from everything else that has been set when an application has not supplied one, so a device does not have to describe itself twice |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetMuid(functionBlockNumber)` | The identifier for a function block, drawn on first use |
| `RegenerateMuid(functionBlockNumber)` | Draws a new identifier and withdraws the old one. The specification requires this when a function block's group placement or group count changes |
| `SetProgramList(resourceId, programList)` | Publishes a program list. A device may publish more than one, told apart by resource id; pass an empty id for a device with only one. Pass null to remove it |
| `GetProgramList(resourceId)` | The program list published under that resource id, or null |
| `SetResource(resource, resourceId, jsonData)` | Publishes anything this API has no type for, as the JSON text of the resource. This is also how an application publishes a resource from a specification the API does not model |
| `RemoveResource(resource, resourceId)` | Removes a resource published that way |
| `SetResourceSubscribable(resource, canSubscribe)` | Lets initiators ask to be told when this resource changes instead of polling for it. Off for everything until set, because a device that accepts a subscription is promising to send updates and should only promise that if it will |
| `IsResourceSubscribable(resource)` | Whether subscriptions to this resource are accepted |
| `NotifyResourceChanged(resource, resourceId)` | Sends the resource to everyone subscribed to it. Call it after changing the resource, not before. Returns how many subscribers were told |
| `SetProfiles(deviceId, enabledProfiles, disabledProfiles)` | The profiles at one address. `deviceId` is `0x00` to `0x0F` for a channel, `0x7E` for a group, or `0x7F` for the whole function block |
| `SendProfileEnabledReport(functionBlockNumber, deviceId, profileId, channelCount)` | Broadcasts that a profile became enabled |
| `SendProfileDisabledReport(functionBlockNumber, deviceId, profileId, channelCount)` | Broadcasts that a profile became disabled |
| `SendProfileAddedReport(functionBlockNumber, deviceId, profileId)` | Broadcasts that this device gained a profile |
| `SendProfileRemovedReport(functionBlockNumber, deviceId, profileId)` | Broadcasts that this device lost a profile |

## Events

| Event | Description |
| ----- | ----------- |
| `MessageReceived` | Every capability inquiry message this responder did not answer itself, with its payload intact. An application that wants to implement something the API does not, or to watch what it is being asked, handles this |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/capability-inquiry-virtual-device/main_capability_inquiry_virtual_device.cpp)
* [C# Sample](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/capability-inquiry-virtual-device/Program.cs)
