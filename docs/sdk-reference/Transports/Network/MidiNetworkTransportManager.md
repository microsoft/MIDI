---
layout: sdk_reference_page
title: MidiNetworkTransportManager
namespace: Windows.Devices.Midi2.Transports.Network
type: runtimeclass
description: The primary class used to create, remove, and monitor Network MIDI 2.0 hosts and client connections
---

The entry point for all Network MIDI 2.0 management. All members are static.

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | Returns true if the Network MIDI 2.0 transport is available in the service. |
| `TransportId` | Returns the GUID of this transport. |
| `MidiNetworkUdpDnsServiceType` | The DNS-SD service type used by Network MIDI 2.0, for use with your own discovery code. |
| `MidiNetworkUdpDnsSdQueryString` | The AQS query string for enumerating Network MIDI 2.0 hosts through `Windows.Devices.Enumeration`. |
| `MidiNetworkUdpDnsDomain` | The DNS-SD domain used for discovery. |
| `MidiNetworkUdpDnsSdDeviceInformationKind` | The `DeviceInformationKind` to use when enumerating with the query string above. |
| `MidiNetworkUdpDnsSdQueryAdditionalProperties` | The additional properties to request when enumerating, so that the returned `DeviceInformation` carries the host name, port, and MIDI-specific fields. |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateNetworkHostAsync(creationConfig)` | Create a host which remote clients can connect to. Returns a `MidiNetworkHostCreationResponse`. Does not complete until the host has started. |
| `RemoveNetworkHostAsync(removalConfig)` | Remove a host and disconnect anything connected to it. Returns a `MidiNetworkHostRemovalResponse`. |
| `StartNetworkHostAsync(hostId)` | Start a host which is configured but stopped. Returns a `MidiNetworkHostUpdateResponse`. |
| `StopNetworkHostAsync(hostId)` | Stop a running host without removing its configuration. Returns a `MidiNetworkHostUpdateResponse`. |
| `ConnectNetworkClientAsync(connectConfig)` | Connect to a remote host, by discovery or by direct address. Returns a `MidiNetworkClientConnectResponse`. For a `ClientId` which already exists, this retries an entry which was previously marked unavailable. |
| `DisconnectNetworkClientAsync(disconnectConfig)` | Disconnect a client connection. Returns a `MidiNetworkClientDisconnectResponse`. A client disconnected this way is not reconnected automatically. |
| `ApproveOrDenyRemoteClientConnectRequestAsync(approvalConfig)` | Approve or deny a remote client which is waiting on a host that requires approval. Returns a `MidiNetworkRemoteClientApprovalResponse`. |
| `GetConfiguredHosts()` | Returns a collection of `MidiNetworkConfiguredHost` for every host configured in this service instance. |
| `GetConfiguredClients()` | Returns a collection of `MidiNetworkConfiguredClient` for every configured client, connected or not. |
| `GetPendingRemoteClients()` | Returns a collection of `MidiNetworkPendingRemoteClient` waiting for a user decision. Poll this to drive an approval UI. |
| `GetAdvertisedHosts()` | Returns a one-shot snapshot of `MidiNetworkAdvertisedHost` currently visible on the network. For ongoing updates use `MidiNetworkAdvertisedHostWatcher`. |

## Remarks

All the `Async` methods are genuinely asynchronous and marked `noexcept` in the ABI. They report failure through the `Success` and `ErrorCode` properties on the returned response object rather than by throwing.

`GetConfiguredClients()` is driven by configuration rather than by live connections, so an entry which has never connected, or which is currently unreachable, still appears in the results with its `EntryState` set accordingly.
