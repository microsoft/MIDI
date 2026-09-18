---
layout: kb
title: How to read a device's patch list
audience: developers
description: Using MIDI Capability Inquiry to show a person the names of the sounds on their instrument, instead of a list of numbers
---

A program change is a number. Nobody thinks in numbers. If your application shows `Program 42` where the instrument's own screen says `Warm Pad`, the person using it has to keep a chart, and every instrument needs a different chart.

MIDI Capability Inquiry is how the instrument tells you the names itself. It is part of MIDI 2.0, it works over any transport Windows MIDI Services supports, and Windows ships a General MIDI synthesizer that answers it, so you can build against something real before you have hardware on the desk.

This article covers reading a device's lists. The [`Windows.Devices.Midi2.CapabilityInquiry`]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/) reference has the full surface.

## What a device publishes

A device that implements Property Exchange publishes named **resources**, each of which is a piece of JSON. The ones worth knowing about are:

| Resource | What it holds |
| -------- | ------------- |
| `ResourceList` | Which other resources this device has, and what may be done with each |
| `DeviceInfo` | Manufacturer, family, model and version, as names rather than as numbers |
| `ChannelList` | What each channel is set to, and which program list that channel selects from |
| `ProgramList` | The programs themselves: a title, the bank and program change numbers that select it, and usually some tags |

A device may publish more than one program list, and point different channels at different ones. A workstation typically has a factory list, a user list and one per expansion, so you cannot assume there is only one.

## Asking

Create a session over a connection you have already opened. The session owns an identifier, numbers its own requests, reassembles chunked replies, and gives up on a device that is not going to answer, so none of that is yours to write.

```cpp
auto session = MidiSession::Create(L"Patch Browser");
auto connection = session.CreateEndpointConnection(endpointId);
connection.Open();

auto capabilityInquiry = MidiCapabilityInquirySession::Create(connection);

auto responders = capabilityInquiry.DiscoverAsync().get();
```

`DiscoverAsync` is the one call that always waits out its whole timeout. There is no way to know how many devices are out there until they have all had a chance to answer, and one endpoint can hold several responders, because an identifier belongs to a function block rather than to a device.

Nothing coming back is an ordinary outcome. It means the endpoint does not implement Capability Inquiry, which most MIDI 1.0 hardware does not. Fall back to whatever you do today.

## Reading the lists

Check what the responder said it can do before asking it anything:

```cpp
if (!responder.SupportsPropertyExchange())
{
    // Nothing to ask for. Fall back.
}
```

Then ask. Each of these is the general request with the header already written and the reply already parsed:

```cpp
auto deviceInfo = capabilityInquiry.GetDeviceInfoAsync(responder.Muid()).get();
auto channels = capabilityInquiry.GetChannelListAsync(responder.Muid()).get();
```

To reach the programs, follow the links from the channel list. `GetProgramListLinks` returns each distinct list once, so a sixteen channel device is not asked for the same collection sixteen times:

```cpp
for (auto const& link : channels.GetProgramListLinks())
{
    auto programs = capabilityInquiry.GetProgramListAsync(responder.Muid(), link.ResourceId()).get();

    for (auto const& program : programs.Entries())
    {
        // program.Title() is what to show.
        // program.BankMsb(), BankLsb() and ProgramChange() are what to send.
    }
}
```

If you only want the programs one channel can select, use `channels.GetEntryForChannel(...)` and read the links off that entry instead. Note that the channel numbers in a `ChannelList` are **one based and run from 1 to 256**, because the specification counts across all sixteen groups rather than restarting at each one.

A device with a single program list may not publish a channel list at all. That is not an error; ask for `ProgramList` with an empty resource id.

## Sending what you found

The three values in a program list entry go on the wire exactly as they appear. They are zero based, and they are Bank Select MSB, Bank Select LSB and Program Change in that order. Do not add one to them for display and then forget to take it off again before transmitting.

## Paging, and why you get it for free

A workstation's program list can run to several thousand entries, far more than fits in one System Exclusive message. The device splits it and expects the initiator to ask for the rest.

`GetProgramListAsync` asks for pages until the device says there are no more, so what comes back is the whole list. If you would rather drive that yourself, for example to show the first page while the rest arrives, use `GetProgramListPageAsync` and follow `HasMoreEntries` and `NextOffset`.

## When a device says no

There are two different refusals and they mean different things.

A **negative acknowledgment** means the transaction failed. `Status` is `NegativeAcknowledgment` and `NakStatusMessage` holds the device's own explanation. The specification asks that you show that text to the person using your application, so show it.

A **reply header status other than 200** means the device answered but would not give you that resource. `ResourceStatus` holds the number; 404 means it does not have it.

**No answer at all** is `NoResponse`. A device is allowed to ignore a request it does not implement, so this is common and is not a fault in the cable or in the service.

## Keeping up with changes

A device can change its mind while you are connected. Profile reports are broadcast, so they arrive whether or not you asked for anything:

```cpp
capabilityInquiry.ProfileStateChanged([](auto&&, auto const& args)
    {
        // args.Message().ProfileId() and ProfileChannelCount()
    });
```

Anything else that arrives unasked, including messages this API has no type for, is raised on `MessageReceived` with its payload intact.

## Things worth knowing

The identifier a device answers to is drawn fresh every time it powers up, and the specification requires that it not be derived from anything stable. **Do not store one and expect to recognize the device by it later.** Use the endpoint device id for that.

A session does not own the connection and does not close it. Several sessions can share one connection, each with its own identifier, which is how you talk to more than one function block on the same endpoint. Closing a session withdraws its identifier, which tells the device to stop tracking it.

Capability Inquiry travels as System Exclusive inside UMP. There is no bytestream path, so an application on WinMM cannot do any of this.

## Related

* [`Windows.Devices.Midi2.CapabilityInquiry` reference]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/)
* [Browsing a device: C++ sample](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/capability-inquiry-browse)
* [Browsing a device: C# sample](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/capability-inquiry-browse)
* [Answering Capability Inquiry from your own application]({{ site.baseurl }}/sdk-reference/CapabilityInquiry/MidiCapabilityInquiryDeviceResponder)
