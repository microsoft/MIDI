---
layout: sdk_reference_page
title: MidiCapabilityInquirySession
namespace: Windows.Devices.Midi2.CapabilityInquiry
type: runtimeclass
implements: Windows.Foundation.IClosable
description: Asks capability inquiry questions over an endpoint connection and waits for the answers
---

A session is an initiator. It owns one identifier for as long as it is open, numbers its own requests, puts chunked replies back together and matches each one to the request that caused it. Anything which arrives that it did not ask for is raised as an event instead, so an application can act on a device announcing a change without having to poll for it.

The session does not take ownership of the connection and does not close it. Several sessions may share one connection, each with its own identifier, which is how an application talks to more than one function block on the same endpoint.

The identifier is drawn at random when the session is created. The specification requires that one must not survive a restart or be derived from anything stable about the machine, so it is not something an application should store or recognize later. Closing the session withdraws it.

Every request returns an `IAsyncOperation`. Each chunk that arrives earns the transfer more time rather than the whole transfer sharing one deadline, because a long list from a busy device is slow rather than absent.

## Properties

| Property | Description |
| -------- | ----------- |
| `SourceMuid` | This session's own identifier, drawn when it was created |
| `Group` | Which group capability inquiry traffic goes out on. Group 1 unless set otherwise |
| `ResponseTimeoutMilliseconds` | How long to wait for an answer before giving up on it. Two seconds by default, which is long enough for a busy device and short enough that a device which is not going to answer does not hold up a user interface |
| `Identity` | What this session declares about itself when it announces itself with Discovery |
| `IsOpen` | False once the session has been closed |

## Methods

| Method | Description |
| ------ | ----------- |
| `GetResponders()` | Every responder found so far, including any which announced themselves rather than being asked |
| `GetResponder(muid)` | One responder by identifier, or null |
| `DiscoverAsync()` | Broadcasts Discovery and collects every reply that arrives before the timeout. Unlike the other requests this always waits the whole timeout, because there is no way to know how many devices are out there until they have all had a chance to answer |
| `SendInvalidateMuid()` | Withdraws this session's identifier. Sent automatically on close, so an application only needs this to release the identifier earlier |
| `RequestPropertyExchangeCapabilitiesAsync(destinationMuid)` | Asks the responder how many requests it will take at once, and records the answer on the responder. The session does this by itself before the first property request it makes to a responder, so an application only needs this to ask a second time |
| `GetPropertyDataAsync(destinationMuid, header)` | The general request. The header names the resource and carries any options. The reply comes back with every chunk already put together |
| `SetPropertyDataAsync(destinationMuid, header, body)` | Sends a resource to the device, chunked to fit what it said it can receive |
| `GetResourceListAsync(destinationMuid)` | Asks for `ResourceList` and returns it as a [`MidiResourceList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiResourceList) |
| `GetDeviceInfoAsync(destinationMuid)` | Asks for `DeviceInfo` and returns it as a [`MidiDeviceInfo`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiDeviceInfo) |
| `GetChannelListAsync(destinationMuid)` | Asks for `ChannelList` and returns it as a [`MidiChannelList`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiChannelList) |
| `GetProgramListAsync(destinationMuid, resourceId)` | Asks for pages of a program list until the device says there are no more, so what comes back is the whole list. Pass an empty resource id for a device with only one list |
| `GetProgramListPageAsync(destinationMuid, resourceId, offset, limit)` | One page of the same list, for an application which would rather page itself |
| `GetProfilesAsync(destinationMuid, deviceId)` | Asks what profiles exist at an address. `deviceId` is `0x00` to `0x0F` for a channel, `0x7E` for a group, or `0x7F` for the whole function block |
| `SendSetProfileOn(destinationMuid, deviceId, profileId, channelCount)` | Asks a device to enable a profile. A device answers with a broadcast report rather than a direct reply, and may refuse, so this returns once the message has been sent |
| `SendSetProfileOff(destinationMuid, deviceId, profileId)` | Asks a device to disable a profile |
| `Close` | (From `IClosable`) Withdraws the identifier, stops listening, and wakes anything still waiting |

## Events

| Event | Description |
| ----- | ----------- |
| `ProfileStateChanged` | Raised for a profile added, removed, enabled or disabled report. These are broadcast, so they arrive whether or not this session asked for anything |
| `ResponderFound` | Raised when a responder answers Discovery, including outside a `DiscoverAsync` call |
| `MessageReceived` | Every capability inquiry message which was not matched to an outstanding request. A subscription update, a device's own Discovery, and anything this API has no strong type for all arrive here with their payload intact |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `Create(connection)` | Creates a session over an already open connection, with a new random identifier |
| `Create(connection, identity)` | The same, for an application which is also a device and already has an identity to declare |

## Examples

* [C++ Sample](https://github.com/microsoft/MIDI/blob/main/samples/cpp-winrt/capability-inquiry-browse/main_capability_inquiry_browse.cpp)
* [C# Sample](https://github.com/microsoft/MIDI/blob/main/samples/csharp-net/capability-inquiry-browse/Program.cs)
