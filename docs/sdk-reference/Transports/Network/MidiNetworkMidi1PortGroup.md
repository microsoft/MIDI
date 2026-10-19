---
layout: sdk_reference_page
title: MidiNetworkMidi1PortGroup
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: A group on a Network MIDI 2.0 connection that gets MIDI 1.0 ports when the device doesn't describe itself, and what those ports are called
---

Added in version 2 of `MidiTransportsNetworkApiContract`.

Used in the `FallbackMidi1PortGroups` list of [MidiNetworkClientConnectConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientConnectConfig/), [MidiNetworkClientUpdateConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkClientUpdateConfig/), [MidiNetworkHostCreationConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostCreationConfig/), [MidiNetworkHostUpdateConfig]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkHostUpdateConfig/) and [MidiNetworkRemoteClientSettings]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkRemoteClientSettings/). Reported by [MidiNetworkSavedClient]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSavedClient/) and [MidiNetworkSavedHost]({{ site.baseurl }}/sdk-reference/Transports/Network/MidiNetworkSavedHost/).

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiNetworkMidi1PortGroup()` | Group 1, with both ports and the name Windows chooses |
| `MidiNetworkMidi1PortGroup(group, name)` | This group, with both ports and this name |
| `MidiNetworkMidi1PortGroup(group, name, hasSourcePort, hasDestinationPort)` | This group, with this name and these ports |

## Properties

| Property | Description |
| -------- | ----------- |
| `Group` | The group, as a [MidiGroup]({{ site.baseurl }}/sdk-reference/MidiGroup/). Group 1 by default. Only the group's index is kept, so changing the `MidiGroup` afterwards changes nothing here |
| `Name` | What MIDI 1.0 apps call this group's ports. Empty means Windows names them after the endpoint, with the group number. Control characters are removed, spaces at either end are trimmed, and the name is cut to 31 characters, which is the longest port name a MIDI 1.0 app can show. Read it back to see the name that will be used |
| `HasSourcePort` | True for a MIDI 1.0 source port, which apps open as a MIDI input to receive what the device sends. True by default |
| `HasDestinationPort` | True for a MIDI 1.0 destination port, which apps open as a MIDI output to send to the device. True by default |

## Remarks

A Network MIDI 2.0 device normally describes its groups with function blocks, and Windows creates MIDI 1.0 ports from those. Some devices never do, such as another Windows PC. For those, a list of these says which groups get ports, in which direction, and what the ports are called. Windows creates them as soon as the connection opens. The list is used instead of `FallbackMidi1PortCount` when it isn't empty.

The device's own function blocks always come first. Once discovery finishes, a device that describes itself gets ports for its own groups instead, and a name given here still applies to a port it has on the same group.

The list is cleaned the same way everywhere it's used. A group that's listed twice keeps the later entry, and a group with neither port is left out, because it would create nothing.

A transport from before this was added ignores the list and uses `FallbackMidi1PortCount`. To work with both, also set `FallbackMidi1PortCount` to one more than the highest group index in the list. To find out whether the installed transport takes the list, call `MidiServiceTransportPluginConfigManager.QueryCapability` with the transport's id and `fallbackMidi1PortGroups`.

```csharp
var config = new MidiNetworkClientConnectConfig();

// ... ClientId, MatchCriteria and so on

// group 1 both ways, and only a destination (Out) on group 2
config.FallbackMidi1PortGroups.Add(new MidiNetworkMidi1PortGroup(new MidiGroup(0), "Juno"));
config.FallbackMidi1PortGroups.Add(new MidiNetworkMidi1PortGroup(new MidiGroup(1), "TR-8", false, true));

// for a transport which only reads the count
config.FallbackMidi1PortCount = 2;
```

See [How Network MIDI 2.0 works in Windows]({{ site.baseurl }}/kb/network-midi2-transport/#midi-10-ports).
