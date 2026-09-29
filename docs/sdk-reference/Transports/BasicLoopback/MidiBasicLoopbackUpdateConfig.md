---
layout: sdk_reference_page
title: MidiBasicLoopbackUpdateConfig
namespace: Windows.Devices.Midi2.Transports.BasicLoopback
type: runtimeclass
implements: Windows.Devices.Midi2.ServiceConfig.IMidiServiceTransportPluginConfig
description: Changes a basic loopback that already exists
---

Changes a basic loopback that already exists. Only the properties you set are changed. Everything else stays as it is.

Pass it to `MidiBasicLoopbackManager.UpdateLoopback` to change the running loopback. Pass it to `MidiServiceTransportPluginConfigManager.SaveUpdate` to change the saved loopback.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBasicLoopbackUpdateConfig(associationId)` | Creates a configuration that changes the basic loopback with this association id |

## Properties

| Property | Description |
| -------- | ----------- |
| `AssociationId` | The GUID of the basic loopback to change |
| `Name` | The new name. An empty name is ignored, because every endpoint needs one |
| `Description` | The new description. Set it to empty to remove the description |
| `ImageFileName` | The bare file name of a picture in the shared endpoint assets folder, not a path. Set it to empty to remove the picture |
| `IsMuted` | True to mute the loopback, false to unmute it |
| `FeedbackProtection` | What the loopback does if MIDI feeds back into it |

## Remarks

The unique id isn't here, because it decides how the endpoint is built. To change it, remove the loopback and create it again.

`UpdateLoopback` stops at the first change that fails, and reports it.

Saving is refused with `ErrorEntryNotSaved` when the loopback isn't saved. That keeps half an entry out of the configuration file.
