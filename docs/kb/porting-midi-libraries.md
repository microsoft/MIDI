---
layout: kb
title: Porting a MIDI Library or Framework to Windows MIDI Services
audience: developers
description: Guidance for maintainers of cross-platform MIDI libraries, language bindings, and application frameworks which wrap the operating system MIDI API
---

<!-- Short link for this page: aka.ms/MidiLibraryPorting -->

# Porting a MIDI Library or Framework to Windows MIDI Services

This article is for you if you maintain something that other people build applications on top of: a cross-platform MIDI library, a language binding, a game engine subsystem, or an application framework. It assumes you already read [Moving from WinMM to Windows MIDI Services]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/), and it covers only the parts that are different when you are the layer in the middle rather than the application.

The difference is not academic. That article opens by telling you to stop thinking in terms of ports. You cannot do that. Your public API almost certainly has a type called `MidiIn` or `MidiOutputPort` or `open_port(index)`, and there are applications in the world pinned to it. Breaking that to model UMP endpoints faithfully is usually not an option, and we are not going to pretend otherwise. What follows is how to sit honestly on top of endpoints while continuing to present ports to your callers, and which of your existing habits will now cause defects.

We would rather help you get this right than debug it later through a customer's bug report. If something here does not fit your library's shape, please [open an issue](https://github.com/microsoft/MIDI/issues) and tell us. Several sections below exist because a library author told us their constraints and we had no good answer written down.

## Target the in-box API

**Use `Windows.Devices.Midi2`.** It is part of Windows, in box in Windows 11 25H2 and later from the end of November 2026, and it is the only supported Windows MIDI Services API target if you intend to ship. This is not a choice to weigh; it is where you are going.

You will find sample code, blog posts, and existing library backends which reference `Microsoft.Windows.Devices.Midi2` instead. That was the out-of-band preview package. **It was never supported for redistribution, only for testing against the preview, and it is being dropped entirely in November 2026.** If your library currently targets it, moving to the in-box namespace is the first piece of work to do, ahead of everything else in this article.

The type names in the two namespaces are largely identical, which makes this more dangerous than it sounds. **Do not copy sample code from one to the other without changing the namespace, and never mix the two in a single build.** A mixed build reports type mismatches which point at the wrong line, and the mistake survives review because every individual line looks correct.

Note also that Windows 11 24H2 leaves support in October 2026, and so will not receive the in-box API or any further updates and fixes to the in-box transports. That boundary is what your library's documented version floor needs to reflect. See [minimum requirements]({{ site.baseurl }}/kb/minimum-requirements/).

## The decision to make before you write any code: COM Extensions or the WinRT connection API

This one is genuinely a fork rather than a preference, and making it late is expensive, because it changes the shape of your receive path and your object model. The COM Extensions and the WinRT message-processing plugins are mutually exclusive on a given connection: when a connection has a registered `IMidiEndpointConnectionMessagesReceivedCallback`, it bypasses all other message handling, including every message listener and the connection's own `MessageReceived` event.

**If you already have UMP parsing code that operates on a stream of 32-bit MIDI words, and you want to send and receive multiple messages per call, use the COM Extensions.** They are the fastest and lightest path: no allocations, buffer-oriented access, and a group of messages that arrived together stays together in a single callback rather than being fanned out into one event per message. Most cross-platform libraries already have exactly this code, because it is what they need for every other platform.

**Otherwise, use the WinRT send methods and the single-message `MessageReceived` event.** This path gives you the message listeners, including `MidiGroupEndpointListener`, and it is the only path on which you can implement a virtual device.

| | COM Extensions | WinRT connection API |
| --- | --- | --- |
| Messages per receive callback | One or more | Exactly one |
| Allocations on the receive path | None | Per message |
| Message listeners (`MidiGroupEndpointListener` and friends) | Not available | Available |
| Virtual device support | Send only | Full |
| Group filtering for port emulation | You write it | `MidiGroupEndpointListener` |
| Languages | C++, Delphi, anything COM-aware | Any WinRT language |

The consequence that catches people out: **if you take the COM path, you have to do your own group filtering**, because listeners will not fire. In practice this is not a hardship for a library, and is often what you want anyway. The callback hands you the session id and the connection id, so you already have to route the buffer back to your own object. Filtering on the group nibble of each message while you are walking the buffer costs you almost nothing, and it keeps the mapping from UMP to your port objects in one place that you control.

> **The two approaches are mutually exclusive, and the API tells you so.** `AddMessageProcessingPlugin` returns `MidiMessageProcessingPluginAddResult::FailedRawCallbackRegistered` if a raw callback is already registered, and `SetMessagesReceivedCallback` returns `E_ILLEGAL_STATE_CHANGE` if plugins are already attached and `E_ILLEGAL_METHOD_CALL` if the connection is already open. Whichever you set up first wins. **Check both return values**, and note that because `MidiVirtualEndpointDevice` is itself a listener, ignoring them on a virtual device gives you something which sends correctly and never receives, with nothing to explain it.

The choice is per connection, not per process, so a library could use COM extensions for normal traffic and the WinRT path for a connection which hosts a virtual device. That works, but it means maintaining two receive paths, so do it deliberately rather than by accident.

See [the COM Extensions reference]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions) and [`MidiEndpointConnection`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection).

## Modeling your ports on top of endpoints

### A port is an endpoint plus a group plus a direction

The address of a MIDI message in UMP is the endpoint, the group, and the channel. It is not a block. Function blocks and group terminal blocks are *metadata describing* groups, and a single block can span several groups.

**Store the endpoint device id and the group index in your port object. Do not store a block number as the port's address.** The reason is that a block number is not a group number and there is no arithmetic that turns one into the other. A group terminal block's `Number()` is its own identifier; the groups it covers are a separate property. If you key on the block number you will eventually index into the wrong group, and on a device where the two happen to line up you will not notice until a customer has a device where they do not.

A useful shape:

```cpp
struct port_address
{
    std::wstring endpoint_device_id;  // stable, and what you reopen with
    uint8_t      group_index;         // 0-15, the actual message address
    direction    dir;                 // input or output, from the block
};
```

Everything else about a port, including the name you show, is display metadata that you should be prepared to refresh.

### Open one connection per endpoint, and share it

**Open exactly one `MidiEndpointConnection` per endpoint, no matter how many of your port objects refer to it, and refcount it.** Each open connection allocates a cross-process memory-mapped buffer between your process and the service, and adds one more client that the service walks on every message for that endpoint. A MIDI 1.0 device with eight inputs and eight outputs is one endpoint with sixteen group/direction combinations, so a library that opens a connection per port turns that device into sixteen buffers and sixteen clients where one would do.

This is the single most common mistake we see in libraries, and it is easy to make, because the natural translation of "the application constructed a `MidiIn`" is "open something." Resist it. Construct your port object cheaply, and open or reuse the shared connection when the application actually opens the port.

```
your library
  session (one, see below)
    connection to endpoint A  <- refcount 3
        port A/group 0 in
        port A/group 0 out
        port A/group 4 in
    connection to endpoint B  <- refcount 1
        port B/group 0 in
```

Close the connection when the last port referring to it closes. Do not close it when any one port closes, because the other ports sharing it would stop receiving with no explanation your caller could act on.

### Filtering input down to one port

On the **WinRT path**, add a `MidiGroupEndpointListener` per open input port, set `IncludedGroups` to the single group that port represents, and wire that listener's `MessageReceived` to the callback your application registered. Set `PreventFiringMainMessageReceivedEvent(true)` if you also handle the connection's own event, so that a message is not delivered twice. Messages without a Group are not delivered to a `MidiGroupEndpointListener`, so stream messages will be silently discarded if you do not have another listener specifically for them, or handle the main `MessageReceived` event.

```cpp
midi2::MidiGroupEndpointListener listener;
listener.IncludedGroups().Append(midi2::MidiGroup(groupIndex));
listener.PreventFiringMainMessageReceivedEvent(true);
listener.MessageReceived({ this, &my_port::on_message });

connection.AddMessageProcessingPlugin(listener);
connection.Open();   // add plugins before opening, or you will miss early messages
```

On the **COM path**, filter in your callback. The group index is the second nibble of the first word of every group-scoped message:

```cpp
// Sketch. Messages with no group (endpoint-scoped, message type 0xF) must not be
// routed to a port; they describe the whole endpoint, not a cable.
const uint8_t messageType = static_cast<uint8_t>((word0 & 0xF0000000) >> 28);
const uint8_t group       = static_cast<uint8_t>((word0 & 0x0F000000) >> 24);
```

> If you prefer, the `Windows.Devices.Midi2.Utilities.MidiMessageHelper` type includes helper functions for getting the Group from a message, and identifying if a message even has a Group field.

**Do not open a second connection to the same endpoint just to get a second filtered stream.** That is what listeners and your own filtering are for, and the cost is the buffer and service client described above.

### Sending to a port

There is no destination parameter. **Set the group in the message itself and send it on the shared connection.** If your public API takes MIDI 1.0 bytes, convert them with `MidiMessageConverter` in `Windows.Devices.Midi2.Utilities.Messages`, which takes the `MidiGroup` as an argument, and use the overload with a `MidiBytestreamToUmpMessageConverterState` when the caller can split a SysEx message across several calls. That state object is what remembers you are mid-SysEx between calls; without it, a continuation buffer will be parsed as though it were the start of a new message.

### Naming ports for your callers

Your users will look for something that resembles the WinMM name they are used to. Endpoint name plus group number plus block name gets you close, and is honest:

```
Contoso Synth (Group 1, Synth Engine)
```

**Do not use a name as a persistent identifier, even though WinMM effectively forced you to.** Users can now rename endpoints, which was one of the most requested features for thirty years, and a library which keys its saved state on the name punishes users for doing it. Persist the endpoint device id and the group index instead, and use the name for display only. If your public API only exposes names, consider adding an identifier accessor so that applications built on you have a way to do the right thing.

Also note that group and channel values are indexed from 0 but are displayed as 1 through 16. The `MidiGroup` and `MidiChannel` types do that conversion for you.

## Session ownership

**Create one session for your library, lazily, and hold it for the lifetime of the library rather than the lifetime of a port.** A session is the unit that MIDI tools show to the user when they ask which applications currently have a device open. Creating one per port object makes that display useless, and tears down and recreates session state as ports come and go.

**Name the session after the host application, not after your library.** If every application built on your library reports its session as `libfoomidi`, then a user looking at the tools to find out what is holding their synth open learns nothing, and neither do we when we are helping them. Add a parameter or a settable property so the application can supply its own name, and fall back to the process name rather than to your library name.

If your library is used by an application which has genuinely separate logical units, such as a DAW with several open projects or a browser with several pages, offer a way to create more than one session, one per unit. That is what sessions are for.

`midi enum sessions` shows the currently active sessions on the PC. Use this when testing to verify the user is presented with what you expect to see. Add `--all` to see sessions with no active connections.

## Enumeration that survives asynchronous arrival

### Use the watcher, not a polling loop

**Use `MidiEndpointDeviceWatcher` and keep your port list up to date from its events. Do not poll.** Under WinMM, `midiInGetNumDevs` was a cheap local call and polling it every 100 ms was rude but survivable. Under Windows MIDI Services, that pattern turns into a steady stream of calls that cross into the service path, and a full sweep that also calls `GetDevCaps` on every device multiplies it by the device count. On a machine with a typical number of ports, a 100 ms sweep is several hundred calls per second, per process, forever, including when the application has no device open and nobody is listening.

Wire your handlers up first, then call `Start()`. `EnumerationCompleted` tells you when the initial list is complete, which is the right moment to hand a first list to the application. `Added`, `Removed` and `Updated` keep it correct after that, and `Updated` matters far more than it did with the older APIs because in-protocol endpoint information and user configuration both cause it to fire.

There is also a `MidiEndpointDeviceWatcher` sibling for legacy ports if you continue to support a WinMM backend: see [Moving from WinMM to Windows MIDI Services]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/).

### Function blocks and group terminal blocks: precedence, not union

This is the rule:

> **When an endpoint declares function blocks, use them and ignore the group terminal blocks. When it declares none, fall back to group terminal blocks. Never merge the two.**

They are two views of the same endpoint at different levels of authority, not two sets of ports. Merging them produces a doubled port list in which the user cannot tell which of two identical-looking entries is the real one.

- **Group terminal blocks** come from USB descriptors, and exist for USB devices only. Windows MIDI Services synthesizes them for USB and Bluetooth LE MIDI 1.0 devices so there is always something to enumerate. They do not currently change at runtime.
- **Function blocks** are discovered in-protocol from the device itself, and are only present on MIDI 2.0 endpoints. They are optional per the specification, but when present they are authoritative and supersede the group terminal blocks. They can be moved and renamed at runtime by design.
- Endpoints which are MIDI 2.0 but not USB, such as Network MIDI 2.0, Bluetooth LE MIDI 2.0, and virtual devices, have function blocks and **no** group terminal blocks at all.

`MidiGroupTerminalBlock::AsEquivalentFunctionBlock()` projects a group terminal block into the function block shape, so you can normalize once at the boundary and keep a single code path afterwards.

**Use the same helper for both listing ports and resolving a port back to something you can open.** If enumeration and resolution consult different collections, every port derived from the collection that resolution does not search will be listed and then fail to open, and which half fails is not something a user could guess.

```cpp
// One helper, used by BOTH your enumeration and your open path, so they cannot disagree.
inline auto blocks_for(MidiEndpointDeviceInformation const& ep)
{
    auto fbs = ep.GetDeclaredFunctionBlocks();

    if (fbs.Size() > 0)
        return normalize(fbs);

    return normalize(ep.GetGroupTerminalBlocks());
}
```

### The first snapshot is not final

An endpoint appears before its properties are complete. For a MIDI 2.0 device, the service exchanges discovery messages with the device and then updates the properties. Until that happens the endpoint is enumerable and has group terminal blocks but no function blocks yet, and then it changes underneath you.

This is the part of the platform library authors tell us is the most frustrating to build on, so it is worth saying plainly why it is not something we can simply fix for you. A device is under no obligation to answer discovery within any particular time, or to send everything it was asked for at all. How long it takes depends on the device, on the transport, and on whatever else the machine happens to be doing. And once discovery is over, function blocks can still be renamed and moved, because the person at the keyboard changed something on the instrument. Those late updates are not a race we lost. They are the user reconfiguring their device, and the service reporting it. An API which handed you a single frozen snapshot would be far easier to consume, and it would be telling your users something untrue about the hardware in front of them.

**Do not try to wait it out.** How long this takes is not something you can predict or bound. It depends on the device, the transport, how busy the machine is, and on a service-side discovery timeout which is configurable and not exposed to you. More importantly, that timeout does not end discovery. It only stops the service waiting for a device which never sends everything it was asked for, and a device is free to send the rest of its discovery responses afterwards. **There is no point at which an endpoint's properties become final**, so a settle timer is wrong in principle, not merely badly tuned.

This means two things for a library with an index-based public API.

**Rebuild your port list on `Updated` when `AreFunctionBlocksUpdated` is set, and keep doing so for as long as you hold the watcher.** Not just during a window after startup. Otherwise a MIDI 2.0 device enumerated before discovery finished keeps its fallback metadata permanently, with names and groupings that do not match what the device actually reports.

**Do not assume a port index is stable across enumerations**, and do not persist one. Indexes were already fragile under WinMM; here the list can legitimately change shape after a device arrives, without anything being plugged or unplugged. Keep offering the index in your public API if your callers need it, but resolve it against the endpoint device id and group you stored, so that a stale index fails cleanly rather than silently opening the wrong device.

If you want to know whether all declared blocks have arrived so far, compare `GetDeclaredEndpointInfo().DeclaredFunctionBlockCount()` against `GetDeclaredFunctionBlocks().Size()`. `DeclaredFunctionBlocksLastUpdateTime` on `MidiEndpointDeviceInformation` is also available. Treat both as a progress indication rather than as a completion signal.

### Filter out what your users should not see

Use `MidiEndpointDeviceInformationFilters::AllStandardEndpoints` unless you have a specific reason not to. The diagnostic loopback endpoints are excluded by default and should stay excluded for anything that is not a test or utility application, because they exist for diagnostics and will confuse a musician looking for their keyboard.

## Never hard-code a transmission limit

There is a limit to how many MIDI words may be sent in a single call, and it is available from `GetSupportedMaxMidiWordsPerTransmission` on both the connection and the COM extension interface.

**Query it per connection and keep it with your connection state. Do not hard-code a constant.** The value is not guaranteed to be the same for every endpoint or for every release, and a hard-coded number silently stops being correct the moment it changes, in a build you shipped years earlier. How this number is generated is an internal Windows implementation detail and is one of the ones most likely to change as we optimize service throughput.

If a call exceeds the limit, the entire call is rejected and nothing is sent, so retrying with a smaller buffer is safe. Split only on message boundaries, so that no UMP ever spans two transmissions, and stop as soon as a transmission fails, because anything already accepted has reached the device and continuing only makes the device's state harder to recover.

**Do this splitting inside your library, not in the applications built on it.** Applications generally cannot reach `GetSupportedMaxMidiWordsPerTransmission` through your abstraction, so if you leave the job to them, they will hard-code a limit, and it will be your users who are affected when it stops being right. This matters most for System Exclusive: a SysEx7 UMP carries six data bytes, so a 64 KB bulk dump is roughly 10,900 UMPs. Firmware updaters and patch librarians all hit this.

Worked examples for both paths are in [`MidiEndpointConnection`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection) and [the COM Extensions reference]({{ site.baseurl }}/sdk-reference/MidiEndpointConnection_COM-Extensions).

## Threading and apartments

Everything in [the threading section of the WinMM article]({{ site.baseurl }}/kb/moving-from-winmm-to-wms/) applies, with one addition that is specific to being a library.

**Initializing the WinRT and COM apartment is per thread, not per process.** You cannot assume the host application has done it, and you cannot assume it has not. If the host already initialized that thread with a different apartment model, your initialization call will fail, and if you treat that as fatal you will break on exactly the hosts that were most careful.

**Do your MIDI work on a thread you own, initialized MTA, rather than on whichever thread the host happened to call you on.** A user interface thread is usually STA, and service startup can take several seconds, which is long enough for the host's window to be reported as unresponsive. Owning the thread also means the apartment model is yours to decide.

If you still have a WinMM backend, there is a second reason the thread should be yours: a `midiOutLongMsg` carrying a large System Exclusive message can block until the transfer has essentially finished. Over a MIDI DIN connection at 31250 baud that is a long time, and it scales with the size of the dump. Firmware updaters and patch librarians are where this shows up.

When shutting down, fully release and reset every COM reference before you uninitialize the apartment. Releasing them in the wrong order, or leaving one alive, can crash at process exit, and that crash will be reported against the host application rather than against you. Uninitializing the apartment is optional, especially when shutting down.

## Habits from WinMM that are now defects

These are patterns we have found in real, shipping libraries. Most of them were survivable under the old stack and are not survivable now, usually because the new stack is faster or because the call crosses a process boundary that previously did not exist.

### Polling the device count

Already covered above. Replace it with the watcher. If you keep a polling fallback for the WinMM backend, do not run it when the Windows MIDI Services backend is active, and do not start the polling thread until the application actually asks to observe device changes.

### Preparing input buffers after opening the port

**Have your buffers and your callbacks in place before you open, not after.** The new stack delivers the first incoming message considerably faster than the old one, so a device that transmits immediately on open can produce messages before a late buffer setup completes. This is a genuine source of application compatibility problems we saw during rollout, and it usually presents as a small number of messages lost at the start of a session rather than as an obvious failure.

The same is true on the new API: add your listeners or register your COM callback before calling `Open()`, because messages begin flowing at `Open()`.

### Freeing a `MIDIHDR` buffer without unpreparing it

**Unprepare the header before you free the buffer, and act on what unprepare tells you.** The usual shortcut is to free on the success return of `midiOutLongMsg` and never unprepare at all. Some libraries unprepare only when the send *fails*, which is backwards: the failing path is the one where the driver never took the buffer in the first place.

The contract is that the driver may still own the buffer after `midiOutLongMsg` returns. That is what `MHDR_INQUEUE`, the `MOM_DONE` callback and the `MIDIERR_STILLPLAYING` return value exist to express. **If `midiOutUnprepareHeader` returns `MIDIERR_STILLPLAYING`, the driver has not finished with the buffer and you must not free it**, so discarding that return value throws away the one signal that would have told you.

This matters more for a library than for an application, because you do not control which driver is underneath you. WinMM still supports third-party `.drv` drivers, and how eagerly any particular one consumes a buffer is not something you can probe for or rely on. Write to the contract and you are correct everywhere. Write to the behavior of whichever driver you happened to test against and you are correct until your user installs something else.

### Closing a handle that was never opened

**Initialize your handle members, and make your destructor check before closing.** More than one library declares `HMIDIOUT outHandle;` without an initializer and then calls `midiOutClose(outHandle)` in the destructor. If the object is constructed and destroyed without ever opening a port, that is a close on an indeterminate value. Debug allocators fill memory with a recognizable pattern, so this reproduces reliably in a debug build and only intermittently in a release build, which is exactly the wrong way round for finding it.

This matters more than it sounds, because libraries frequently construct a backend eagerly in order to enumerate, and destroy it without ever opening anything.

### Calling `midiInReset` before `midiInStop`

**Stop first, then reset.** Resetting while the port is still running returns buffers to you while more can still arrive, which produces a race between your buffer bookkeeping and the driver's.

### Retrying `MIDIERR_NOTREADY` in a tight loop

**Back off between retries and give up after a bounded time.** An unbounded spin on `MIDIERR_NOTREADY` burns a core, and because it never terminates, it turns a transient condition into a hang that the application author cannot diagnose. A short sleep with a retry ceiling turns the same condition into an error the application can report.

### Sending a short SysEx continuation through `midiOutShortMsg`

**Route every part of a System Exclusive message through the long-message path, regardless of length.** Some libraries optimize by sending buffers of three bytes or fewer with `midiOutShortMsg`. That path is for complete non-SysEx messages and does not carry SysEx state, so a continuation or terminating fragment that happens to be short is not interpreted as part of the message in progress.

This is easy to miss because it depends on arithmetic. A transfer only trips over it when the total payload size modulo the chunk size happens to land at three or fewer, so the same code can move a hundred firmware images correctly and fail on the next one.

### Acting on a device arrival before its ports exist

**A WinMM port is usable as soon as it is enumerated. What takes time is the port appearing at all.** Endpoint creation, and MIDI 1.0 port creation for that endpoint, do not complete the moment Windows first sees the device. A device which re-enumerates, as one does when it reboots into a bootloader for a firmware update, goes away and comes back on that same schedule. So the thing to wait for is the port showing up in the list, not for an enumerated port to become ready.

That distinction decides where the retry belongs:

- **Do not treat a device's arrival as port availability.** If you trigger your enumeration from hardware or PnP arrival, you will run it before the ports exist and conclude the device has none. Use `MidiEndpointDeviceWatcher`, or the legacy port watcher, which fire when the endpoint is actually published, and be prepared for the port list to grow again shortly after.
- **Do not observe in one API and act in another.** WinMM has a second asynchronous step of its own which assigns port numbers. If you observe through a Windows MIDI Services Endpoint Device watcher and then immediately call a WinMM API, the port number may not be recognized yet. Opening a WinMM port from inside an Endpoint Device watcher's `Added` handler is the specific case that bites. The `MidiLegacyPortDeviceWatcher` is the better watcher to use in this case, but there is still an asynchronous, but fast, caching step in `wdmaud2.drv` for updating the valid values, including port numbers, used by WinMM APIs.

We are working on shortening the endpoint and port creation window. Until then, anything which drives a device through a reboot, such as a firmware updater, should expect a gap where the device exists and its ports do not, and should say it is waiting for the device/ports rather than reporting a hard error. 

The expected delay will be the amount of time the device takes to reboot and become ready + Windows PnP delay (which can be several seconds if the PC has a lot of "dead" PnP entries to wade through) + the service Endpoint creation delay + the service MIDI 1 Port creation delay. All-up, this could be several seconds.

### Assuming exclusive access

**Do not treat a successful open as evidence that nothing else is using the device, and do not treat a failure to open as evidence that something is.** Multi-client is a first-class feature now, and it was the single most requested one. Logic that inferred exclusivity from open behavior was always inferential and no longer holds.

## Things that are true elsewhere and false here

If you are porting from a CoreMIDI or ALSA backend, or if you are using an AI coding assistant which has read a great deal of both, these are the assumptions most likely to be carried over incorrectly.

| Assumption | Reality on Windows MIDI Services |
| --- | --- |
| You open a port | You open a bidirectional connection to an endpoint; groups are addressed within the message |
| Sending takes a destination | Sending takes a connection; the group is a field in the UMP |
| Timestamps are relative to when the port opened | Timestamps are absolute, from system boot, in 100 ns units, and do not wrap |
| SysEx is a byte buffer you hand to the API | SysEx is a sequence of SysEx7 UMPs, six data bytes each |
| Enumeration is synchronous and its result is stable | Enumeration is asynchronous, and a MIDI 2.0 endpoint's properties can change after it appears, with no point at which they are final |
| The device name identifies the device | Names are user-editable display metadata; the endpoint device id identifies the device |
| Opening a device is exclusive | Multi-client is supported and expected |
| One device equals one or two ports | One device is one endpoint with up to 16 groups in each direction |

## Testing without hardware

You can get real coverage in CI on a machine with no MIDI devices attached.

- Call `MidiApi::EnsureServiceAvailable()` first and skip rather than fail if it returns false, so that a machine with the service disabled or in legacy API mode produces a clear skip instead of a confusing failure.
- The two cross-wired diagnostic loopback endpoints are always present when the service is running, and give you a genuine round trip through the service, including the cross-process buffer. That is enough to test your send path, your receive path, your splitting logic and your shutdown ordering. (Send to A, receive on B. Send to B, receive on A)
- Loopback endpoints and virtual devices let you construct multi-group endpoints on demand, which is how you test the group filtering and port emulation described above without owning a device that has eight cables.
- The `midi` console tool that ships with Windows MIDI Services can enumerate endpoints and show properties, which is useful for asserting from a test script what your library should be seeing. `midi endpoint properties <id> --verbose` shows both function blocks and group terminal blocks; without `--verbose` the group terminal blocks are deliberately hidden when function blocks are present, which will mislead you if you are using the console to check your precedence logic.
- Windows MIDI Services runs on Arm64, and Arm64 is a first-class citizen with Windows 11. If your library ships Arm64 binaries, run at least the enumeration and loopback tests there too.

## Checklist

Enumeration and identity:

- [ ] Port list is built from a `MidiEndpointDeviceWatcher`, not a polling loop
- [ ] No polling thread starts unless the application asked to observe device changes
- [ ] Function blocks take precedence over group terminal blocks; the two are never merged
- [ ] The same helper feeds both the enumeration path and the open path
- [ ] The list is rebuilt on `Updated` when `AreFunctionBlocksUpdated` is set, for as long as the watcher is held, not only during a window after startup
- [ ] Nothing waits a fixed amount of time for an endpoint's properties to settle
- [ ] Ports are identified by endpoint device id plus group index, never by name and never by block number
- [ ] Diagnostic endpoints are filtered out

Connections and sessions:

- [ ] One session for the library, named after the host application or an app-supplied string
- [ ] One connection per endpoint, refcounted across all ports that use it
- [ ] Listeners added, or COM callback registered, before `Open()`
- [ ] Closing one port does not close a connection another port is using

Sending:

- [ ] `GetSupportedMaxMidiWordsPerTransmission` is queried per connection, never hard-coded
- [ ] Large transfers are split inside the library, on message boundaries
- [ ] Sending stops at the first failed transmission
- [ ] The group is set in the message, not passed as a destination

Threading and lifetime:

- [ ] Apartment initialization is per thread and a failure is handled, not fatal
- [ ] MIDI work happens on a thread the library owns, not the caller's UI thread
- [ ] All COM references are released before the apartment is uninitialized

If you still have a WinMM backend:

- [ ] Buffers and callbacks are in place before the port is opened
- [ ] Headers are unprepared and completion is observed before buffers are freed
- [ ] Handles are initialized, and the destructor checks before closing
- [ ] `midiInStop` precedes `midiInReset`
- [ ] Retries are bounded and backed off
- [ ] Every part of a SysEx message goes through the long-message path, whatever its length

## Getting help

- The short link for this page is [aka.ms/MidiLibraryPorting](https://aka.ms/MidiLibraryPorting). Use it when citing this article, as it will keep working if the page moves.
- Issues, questions and corrections to this page: [github.com/microsoft/MIDI/issues](https://github.com/microsoft/MIDI/issues)
- Samples in several languages: [aka.ms/midisamples](https://aka.ms/midisamples)
- The Windows MIDI Services Discord is the fastest way to reach the team and other implementers; the invitation link is on the [repository home page](https://aka.ms/midirepo)

If you are partway through a port and something in the API is making your library's shape awkward, tell us before you work around it and we'll see if we can work together on a better solution than a workaround.
