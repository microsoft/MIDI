---
layout: sdk_reference_page
title: MidiLoopbackManager
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
description: The primary class used to create or remove loopback endpoints
---

Creates, removes, mutes, and lists loopback endpoint pairs.

## Static Properties

| Static Property | Description |
| -------- | ----------- |
| `IsTransportAvailable` | True if this transport is available in the service |
| `TransportId` | The GUID of this transport |
| `IsFeedbackProtectionAvailable` | False when the loopback transport on this PC can't watch for feedback |

## Static Methods

| Static Method | Description |
| -------- | ----------- |
| `CreateTransientLoopback(creationConfig)` | Creates a temporary pair of loopback endpoints. They last until you remove them or the service restarts. Returns a `MidiLoopbackCreationResponse` |
| `RemoveTransientLoopback(removalConfig)` | Removes a temporary pair of loopback endpoints. Returns a `MidiLoopbackRemovalResponse` |
| `GetAssociatedLoopbackEndpointForId(loopbackEndpointId)` | Given the device id of one loopback endpoint, returns the `MidiEndpointDeviceInformation` for the other endpoint in its pair |
| `GetAssociatedLoopbackEndpoint(loopbackEndpoint, endpointsToSearch)` | Returns the other endpoint in a loopback pair, looking only in `endpointsToSearch` |
| `GetAssociatedLoopbackEndpoint(loopbackEndpoint)` | Returns the other endpoint in a loopback pair, looking in all current endpoints |
| `GetAssociationId(loopbackEndpoint)` | Returns the association GUID of this loopback endpoint |
| `DoesLoopbackAExist(uniqueIdentifier)` | Returns true if the A side of a loopback with this unique id already exists |
| `DoesLoopbackBExist(uniqueIdentifier)` | Returns true if the B side of a loopback with this unique id already exists |
| `MuteLoopback(associationId)` | Mutes the loopback pair with this association id, so no messages get through. Returns a `MidiLoopbackUpdateResponse` |
| `UnmuteLoopback(associationId)` | Unmutes the loopback pair with this association id. Returns a `MidiLoopbackUpdateResponse` |
| `SetFeedbackProtection(associationId, feedbackProtection)` | Changes what the loopback pair does if MIDI feeds back into it. Takes effect right away. To keep it after a restart, save a `MidiLoopbackUpdateConfig` with `FeedbackProtection` set. Fails with `FeedbackProtectionNotAvailable` when `IsFeedbackProtectionAvailable` is false. Returns a `MidiLoopbackUpdateResponse` |
| `GetActiveLoopbackEntries()` | Returns a `MidiLoopbackEntry` for each active loopback pair |
| `GetSavedLoopbackEntries()` | Returns a [`MidiLoopbackSavedEntry`]({{ site.baseurl }}/sdk-reference/Transports/Loopback/MidiLoopbackSavedEntry/) for each loopback pair saved in the configuration file. These are the pairs the service creates when it starts. Works even when the service isn't running |
| `UpdateLoopback(updateConfig)` | Changes a running loopback pair's names, descriptions, pictures, muted state and feedback protection. Only the properties set in the [`MidiLoopbackUpdateConfig`]({{ site.baseurl }}/sdk-reference/Transports/Loopback/MidiLoopbackUpdateConfig/) change. Returns a `MidiLoopbackUpdateResponse` |

## Remarks

If your app creates endpoints so it can talk to other apps, it should usually use the virtual device support in the API. But sometimes an app needs a simpler loopback endpoint, without the protocol negotiation, MIDI 2.0 discovery, and lifetime management that virtual devices have. That's what loopbacks are for.

Loopback endpoints that people create in the MIDI tools are saved in the configuration, so they stay after the service restarts or the PC reboots. Loopback endpoints created with this API are temporary, and go away when the service restarts. Either way, the loopback transport must be installed and turned on.

## Saved and running loopbacks

A loopback can be running, saved, or both. `GetActiveLoopbackEntries` lists what's running now. `GetSavedLoopbackEntries` lists what's saved. Match the two on `AssociationId`.

To save a loopback, pass its `MidiLoopbackCreationConfig` to `MidiServiceTransportPluginConfigManager.SaveUpdate`. To remove a saved one, pass a `MidiLoopbackRemovalConfig` to the same method.

To change a loopback, make a `MidiLoopbackUpdateConfig`. Pass it to `UpdateLoopback` to change the running pair, and to `SaveUpdate` to change the saved one. Do both if the loopback is running and saved.

```cpp
MidiLoopbackUpdateConfig update{ associationId };
update.EndpointAName(L"Synth In");
update.IsMuted(true);

auto response = MidiLoopbackManager::UpdateLoopback(update);

if (response.Success())
{
    MidiServiceTransportPluginConfigManager::SaveUpdate(update);
}
```
