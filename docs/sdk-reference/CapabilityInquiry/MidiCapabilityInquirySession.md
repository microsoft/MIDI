---
layout: sdk_reference_page
title: MidiCapabilityInquirySession
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IClosable
description: Asks capability inquiry questions over an endpoint connection and waits for the answers
---

A session is the side that asks the questions. The MIDI-CI specification calls it an initiator. It keeps one identifier for as long as it's open, numbers its own requests, puts replies that arrive in pieces back together, and matches each reply to the request that caused it. Anything that arrives without being asked for is raised as an event instead. That way your application can act when a device announces a change, without having to keep asking.

The session doesn't take over the connection, and it doesn't close it. Several sessions can share one connection, each with its own identifier. That's how an application talks to more than one function block on the same endpoint.

The identifier is picked at random when the session is created. The specification says an identifier must not be kept after a restart, or be based on anything fixed about the PC. So don't store it or expect to see it again later. Closing the session withdraws it.

Every request returns an `IAsyncOperation`. Each piece that arrives gives the transfer more time, instead of the whole transfer sharing one deadline, because a long list from a busy device is slow, not missing.

## Properties

| Property | Description |
| -------- | ----------- |
| `SourceMuid` | This session's own identifier, picked when the session was created |
| `Group` | Which group capability inquiry messages are sent on. Group 1 unless you change it |
| `ResponseTimeoutMilliseconds` | How long to wait for an answer before giving up. The default is two seconds, which is long enough for a busy device, and short enough that a device that won't answer doesn't hold up your user interface |
| `Identity` | What this session says about itself when it announces itself with Discovery |
| `SupportedCategories` | Which capability inquiry categories this session says it supports, also sent with Discovery. The default is property exchange and profile configuration, which is right for an application that only asks questions. If your application answers questions too, list only what it really answers |
| `ReceivableMaximumSystemExclusiveSize` | The largest System Exclusive message, in bytes, this session says it can receive, also sent with Discovery. The default is `MidiCapabilityInquiryMessageBuilder.MinimumReceivableSystemExclusiveSize` (512), the smallest the specification allows. A smaller value is raised to that minimum |
| `IsOpen` | False once the session has been closed |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetResponders()` | Every responder found so far, including any that announced themselves without being asked |
| `GetResponder(muid)` | One responder by identifier, or null |
| `DiscoverAsync()` | Sends Discovery to every device and collects each reply that arrives before the timeout. Unlike the other requests, this always waits for the whole timeout, because there's no way to know how many devices are out there until they've all had a chance to answer |
| `SendInvalidateMuid()` | Withdraws this session's identifier. Closing the session does this for you, so you only need it to give up the identifier sooner |
| `RequestPropertyExchangeCapabilitiesAsync(destinationMuid)` | Asks the responder how many requests it will take at once, and saves the answer on the responder. The session does this on its own before its first property request to a responder, so you only need it to ask again |
| `GetPropertyDataAsync(destinationMuid, header)` | The general-purpose request. The header names the resource and holds any options. The reply comes back with all its pieces already put together |
| `SetPropertyDataAsync(destinationMuid, header, body)` | Sends a resource to the device, split into pieces that fit what the device said it can receive |
| `GetResourceListAsync(destinationMuid)` | Asks for `ResourceList` and returns it as a [`MidiResourceList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceList) |
| `GetDeviceInfoAsync(destinationMuid)` | Asks for `DeviceInfo` and returns it as a [`MidiDeviceInfo`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiDeviceInfo) |
| `GetChannelListAsync(destinationMuid)` | Asks for `ChannelList` and returns it as a [`MidiChannelList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiChannelList) |
| `GetProgramListAsync(destinationMuid, resourceId)` | Asks for pages of a program list until the device says there are no more, so what comes back is the whole list. Pass an empty resource id for a device with only one list |
| `GetProgramListPageAsync(destinationMuid, resourceId, offset, limit)` | One page of the same list, for an application which would rather page itself |
| `SubscribeAsync(destinationMuid, resource, resourceId)` | Asks to be told when a resource changes, so you don't have to keep asking. Only try it on a resource whose entry in the device's resource list says `CanSubscribe`. Returns a [`MidiPropertySubscription`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiPropertySubscription) |
| `UnsubscribeAsync(subscription)` | Ends a subscription. Closing the session ends any that are still active |
| `GetSubscriptions()` | Every subscription this session holds that the device hasn't ended |
| `GetProfilesAsync(destinationMuid, deviceId)` | Asks what profiles exist at an address. `deviceId` is `0x00` to `0x0F` for a channel, `0x7E` for a group, or `0x7F` for the whole function block |
| `SendSetProfileOn(destinationMuid, deviceId, profileId, channelCount)` | Asks a device to turn on a profile. The device answers with a report sent to everyone, not a direct reply, and it may refuse. So this returns as soon as the message is sent. Watch `ProfileStateChanged` to see what the device did |
| `SendSetProfileOff(destinationMuid, deviceId, profileId)` | Asks a device to turn off a profile |
| `Close` | (From `IClosable`) Withdraws the identifier, stops listening, and ends any requests that are still waiting |

## Events

| Event | Description |
| ----- | ----------- |
| `ProfileStateChanged` | Raised when a device reports that a profile was added, removed, turned on, or turned off. These reports go to everyone, so they arrive whether or not this session asked for anything |
| `ResponderFound` | Raised when a responder answers Discovery, including outside a `DiscoverAsync` call |
| `PropertySubscriptionUpdated` | Raised when a device reports that a subscribed resource changed, and when it ends a subscription. The reply the specification requires is sent before this is raised |
| `MessageReceived` | Raised for every capability inquiry message that doesn't match a waiting request. A subscription update, a device's own Discovery, and anything this API has no special type for all arrive here, with all their data |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `Create(connection)` | Creates a session on a connection that's already open, with a new random identifier |
| `Create(connection, identity)` | The same, for an application that is also a device and already has an identity to announce |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/capability-inquiry-browse/main_capability_inquiry_browse.cpp)
* [C# Sample](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/capability-inquiry-browse/Program.cs)
