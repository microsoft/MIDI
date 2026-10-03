---
layout: sdk_reference_page
title: MidiRtpSavedHost
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
description: An RTP-MIDI host saved in the configuration file
---

An RTP-MIDI host saved in the configuration file. The service starts it every time it starts. `MidiRtpTransportManager.GetSavedHosts` returns one of these for each saved host.

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | The GUID that identifies this host. It's the same as `MidiRtpConfiguredHost.HostId` when the host is running |
| `Name` | What remote devices show for this PC. Empty means this PC's name |
| `ServiceInstanceName` | The name the host advertises on the network, when it's different from `Name` |
| `IsEnabled` | False when the service shouldn't start the host |
| `UseAutomaticPortAllocation` | True when the host tries 5004 and takes a free port if that one is taken |
| `ManuallyAssignedPort` | The port the host asks for. Zero when `UseAutomaticPortAllocation` is true, or when the saved port can't be used |
| `AllowPortFallback` | True when the host may start on another port if its own is taken |
| `Advertise` | True when the host advertises itself on the network |
| `RemoteClientPolicy` | What the host does when a remote device it hasn't decided about asks to connect |
| `SendRecoveryJournal` | True when the host sends the recovery journal, so a remote device can repair a lost Note Off |
| `SendSpeedLimit` | How fast the host sends to each connected device. See [MidiRtpSendSpeedLimit]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpSendSpeedLimitEnum/) |
| `NetworkAdapterId` | The network adapter the host is limited to, or an empty GUID for every adapter |
| `NetworkAdapterName` | That adapter's name, from when it was chosen |
| `AllowNetworkAdapterFallback` | True when the host runs on every adapter while its own adapter is missing |
| `KnownRemoteClients` | Every saved allow and deny decision, as [MidiRtpKnownRemoteClient]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpKnownRemoteClient/) objects |

## Remarks

This comes from the configuration file, not from the service. It tells you what the service starts the next time it starts, and it works even when the service isn't running. A missing value reads as the default the service uses.

To change a saved decision, start from `KnownRemoteClients`, make the change, and save a [MidiRtpHostKnownClientsConfig]({{ site.baseurl }}/sdk-reference/Transports/Rtp/MidiRtpHostKnownClientsConfig/) holding the whole list.
