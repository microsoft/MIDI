---
layout: sdk_reference_page
title: MidiLoopbackUpdateConfig
namespace: Windows.Devices.Midi2.Transports.Loopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Changes a loopback pair that already exists
---

Changes a loopback pair that already exists. Only the properties you set are changed. Everything else stays as it is.

Pass it to `MidiLoopbackManager.UpdateLoopback` to change the running pair. Pass it to `MidiServiceTransportPluginConfigManager.SaveUpdate` to change the saved pair.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiLoopbackUpdateConfig(associationId)` | Creates a configuration that changes the loopback pair with this association id |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID of the loopback pair to change |
| `EndpointAName` | The new name for the A side. An empty name is ignored, because every endpoint needs one |
| `EndpointADescription` | The new description for the A side. Set it to empty to remove the description |
| `EndpointAImageFileName` | The bare file name of a picture in the shared endpoint assets folder, not a path. Set it to empty to remove the picture |
| `EndpointBName` | The new name for the B side. An empty name is ignored |
| `EndpointBDescription` | The new description for the B side |
| `EndpointBImageFileName` | The picture for the B side |
| `IsMuted` | True to mute the pair, false to unmute it |
| `FeedbackProtection` | What the pair does if MIDI feeds back into it |

## Remarks

The unique ids and the UMP-only setting aren't here, because they decide how the endpoints are built. To change one, remove the loopback and create it again.

`UpdateLoopback` stops at the first change that fails, and reports it. For example, a new name that another endpoint already uses is refused before anything else is changed.

Saving is refused with `ErrorEntryNotSaved` when the pair isn't saved. That keeps half an entry out of the configuration file.

```cpp
MidiLoopbackUpdateConfig update{ associationId };
update.EndpointBName(L"Sequencer Out");
update.EndpointBImageFileName(L"");

auto response = MidiLoopbackManager::UpdateLoopback(update);

if (response.Success())
{
    MidiServiceTransportPluginConfigManager::SaveUpdate(update);
}
```
