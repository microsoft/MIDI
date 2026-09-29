---
layout: sdk_reference_page
title: MidiRtpHostCreationConfig
namespace: Windows.Devices.Midi2.Transports.Rtp
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Config sent to the service to create an RTP-MIDI host
---

Describes a host for remote devices to connect to. Pass it to `MidiRtpTransportManager.CreateRtpHostAsync`. To keep the host after the service restarts, also pass it to `MidiServiceTransportPluginConfigManager.SaveUpdate`.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiRtpHostCreationConfig()` | Creates a configuration with a new `HostId` and the defaults below |

## Properties

| Property | Description |
| -------- | ----------- |
| `HostId` | Read-only. The GUID that identifies this host entry, made when the configuration is created. Use it to stop, start, and remove the host later, and to find it in `GetConfiguredHosts()` |
| `Name` | What remote devices show for this PC. Leave it empty to use this PC's name. At most 63 bytes in UTF-8 |
| `ServiceInstanceName` | The name advertised on the network, when it should be different from `Name`. Leave it empty to advertise `Name`. At most 63 bytes in UTF-8, with no periods |
| `UseAutomaticPortAllocation` | When true, the host tries port 5004 first, and takes a free port if 5004 is in use. When false, `ManuallyAssignedPort` is used |
| `ManuallyAssignedPort` | The UDP port to use, from 1024 to 65534. RTP-MIDI also uses the port after it. Ignored when `UseAutomaticPortAllocation` is true |
| `AllowPortFallback` | Only matters when you choose a port. If that port is in use, the host starts on another one, instead of not starting at all. `MidiRtpConfiguredHost.UsedPortFallback` tells you when that happened |
| `Advertise` | When true, the host is advertised on the local network, so remote devices list it. When false, it can only be reached by its address and port |
| `RemoteClientPolicy` | What the host does when a remote device it hasn't been told about asks to connect. See `MidiRtpRemoteClientPolicy` |
| `SendRecoveryJournal` | When true, each packet carries a recovery journal, so the remote device can repair a lost Note Off. Only turn it off for a device that can't read one |

## Defaults

| Property | Default |
| -------- | ------- |
| `Name` | Empty, so this PC's name is used |
| `ServiceInstanceName` | Empty, so `Name` is advertised |
| `UseAutomaticPortAllocation` | `true` |
| `ManuallyAssignedPort` | `5004` |
| `AllowPortFallback` | `true` |
| `Advertise` | `true` |
| `RemoteClientPolicy` | `AllowAny` |
| `SendRecoveryJournal` | `true` |

## Remarks

The 63 byte limit is in bytes, not characters, so a name with non-ASCII characters holds fewer characters than you might expect. It's the most a name advertised on the network can hold. A longer name fails with `NameTooLong`, instead of being shortened.

When the host is advertised and `ServiceInstanceName` is empty, `Name` is the name advertised, so it can't have a period in it either.

If another device on the network already advertises the same name, Windows gives the host a different one, instead of refusing it. `MidiRtpConfiguredHost.ActualServiceInstanceName` has the name really in use.
