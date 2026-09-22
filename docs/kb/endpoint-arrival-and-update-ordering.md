---
layout: kb
title: Endpoint Arrival and Update Ordering
audience: developers
description: What actually happens when a MIDI device appears, which notifications you get in which order, and which information is safe to read when
categories:
  - Developer Guidance
  - Internals
---

A MIDI 2.0 device doesn't arrive fully described. It arrives, and then it tells us about itself. Windows MIDI Services publishes each piece of that description as it comes in, so an application watching endpoints sees a *sequence* of notifications for a single device being plugged in, not one.

That surprises people. Applications which assume a device is fully described the moment it appears, or the moment any single notification arrives, end up caching half-built information: blank function block names, a MIDI 1.0 port list with a port missing, an endpoint name which is later replaced by the one the device reports.

This article describes what to expect, and how to write a handler that stays correct.

If you haven't used the watcher before, start with [How to Enumerate UMP Endpoints with Add/Remove/Change Notification]({{ site.baseurl }}/kb/how-to-watch-endpoints/).

## What happens when a device appears

For a UMP-native device the rough order is:

1. The transport creates the endpoint. Your `Added` handler runs. At this point the endpoint has the name and capabilities the transport knows about, and for a USB device, its group terminal blocks.
2. The service performs endpoint discovery and protocol negotiation with the device. As replies arrive, endpoint information, device identity, the negotiated protocol, function blocks and function block names are published. **Each of these raises `Updated`.**
3. MIDI 1.0 ports for the endpoint are created, renamed, enabled or removed to match what the device has reported so far. This happens more than once, because it happens again each time more function block information arrives.
4. Discovery completes, or the service stops waiting for a device which isn't answering. `IsEndpointDiscoveryComplete` becomes true and `Updated` is raised with `IsEndpointDiscoveryStateUpdated` set.

A device which isn't UMP-native, such as a MIDI 1.0 USB device, skips step 2 and is largely described at step 1.

None of this is instantaneous, and none of it is synchronized with your process. A device the user switches on while your application is running will walk through the whole sequence.

## The update flags tell you what changed

`MidiEndpointDeviceInformationUpdatedEventArgs` carries a set of `IsXxxUpdated` and `AreXxxUpdated` properties so that you can rebuild only the part of your model which changed. The full list is on the [reference page]({{ site.baseurl }}/sdk-reference/Enumeration/MidiEndpointDeviceInformationUpdatedEventArgs/).

**At least one flag is always set.** Every property the watcher requests belongs to at least one group, so there's no "something else changed" state to write code for. If you're testing all the flags and finding none set, you're running against a build from before this was true; re-read the properties you care about in that case rather than skipping the update.

The groups deliberately overlap. A user-assigned endpoint name sets both `IsNameUpdated` and `IsUserMetadataUpdated`, because it's both of those things. Test the flag which matches what your code does, not the one which matches where the value is stored.

## Function block names arrive separately from function blocks

This is the one which catches most applications.

In the MIDI 2.0 UMP specification, a Function Block Info Notification and a Function Block Name Notification are different messages. A device commonly sends all of its block information first and its block names afterwards, so there's a window in which a function block legitimately exists with no name yet.

Read the name every time you handle `AreFunctionBlocksUpdated`, and treat a blank name as "not yet", not as "this block has no name":

```cpp
for (auto const& block : args.UpdatedDevice().GetDeclaredFunctionBlocks())
{
    if (!block.Name().empty())
    {
        // update your cached name for this block
    }
}
```

Don't cache the blank and stop looking. If you need something to show in the meantime, the group terminal block name or the MIDI 1.0 port name for the same group is a reasonable placeholder.

## Don't enumerate MIDI 1.0 ports from the endpoint `Updated` handler

If you present MIDI 1.0-style ports to part of your application, get them from [`MidiLegacyPortDeviceWatcher`]({{ site.baseurl }}/sdk-reference/Enumeration/Legacy/MidiLegacyPortDeviceWatcher/), not by listing them inside an endpoint update.

The ports are separate device interfaces. The service creates, renames and removes them as function block information arrives, and Windows delivers their arrival notifications independently of the endpoint's. There's no ordering between "the endpoint update reached your process" and "the port that update implies exists". Listing ports inside the endpoint handler is therefore a race you can lose in either direction, and you can lose it on the last update as easily as the first.

`IsMidi1PortMappingUpdated` tells you the mapping changed, which is a good reason to refresh your view. Take the actual list from the port watcher, which has its own `Added`, `Updated` and `Removed` events, and `GetEnumeratedPortsForAssociatedEndpoint()` to relate ports back to the endpoint they came from.

See the [C++/WinRT watch-midi1-ports](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/watch-midi1-ports) and [C# watch-midi1-ports](https://github.com/microsoft/MIDI/tree/main/samples/csharp-net/watch-midi1-ports) samples.

## `IsEndpointDiscoveryComplete` is a hint, not a barrier

`MidiEndpointDeviceInformation.IsEndpointDiscoveryComplete` becomes true when the service finishes gathering in-protocol information, or when it stops waiting for a device which didn't answer. For an endpoint which doesn't use in-protocol discovery it's true from the start.

It's genuinely useful: it's a sensible moment to settle your user interface rather than redrawing on every intermediate update. But:

- **It can stay false.** If the device is removed part way through discovery, nothing sets it. Never block your application, or a user-visible operation, waiting for it.
- **Updates continue afterwards.** A device may send more information later, a user may rename the endpoint in MIDI Settings, and MIDI 1.0 port names can still change. Keep your handlers wired up.
- **It doesn't mean the device answered.** It's also set on timeout. If you need to know whether the device actually described itself, look at what it reported.

## Threading

Watcher events are raised from Windows device enumeration, not from your thread. Depending on the apartment your application uses, handlers can run on a thread pool thread, and successive events for the same endpoint aren't guaranteed to run on the same thread. Protect any state your handler touches, and don't assume the handler is running where your user interface lives.

`UpdatedDevice` refers to the watcher's live object for that endpoint, and the watcher keeps it up to date. If you need a stable picture to work from, take the values you need out of it at the top of your handler rather than reading it repeatedly while you build something.

## Checklist

- [ ] `Added`, `Updated` and `Removed` are all handled, for the lifetime of the watcher, not just during startup
- [ ] The handler re-reads what it needs on every update rather than acting only on the first
- [ ] Function block names are re-read on every `AreFunctionBlocksUpdated`, and a blank name is treated as "not yet"
- [ ] MIDI 1.0 ports come from `MidiLegacyPortDeviceWatcher`, not from inside the endpoint handler
- [ ] Nothing blocks waiting for `IsEndpointDiscoveryComplete`
- [ ] Handler state is safe to touch from a thread pool thread
- [ ] `Removed` is handled, so an endpoint which is removed and re-added doesn't leave a duplicate entry in your list

## Related

* [How to Enumerate UMP Endpoints with Add/Remove/Change Notification]({{ site.baseurl }}/kb/how-to-watch-endpoints/)
* [Porting a MIDI Library or Framework to Windows MIDI Services]({{ site.baseurl }}/kb/porting-midi-libraries/)
* [How MIDI 1.0 Port Names are Generated]({{ site.baseurl }}/kb/how-midi1-port-names-are-generated/)
