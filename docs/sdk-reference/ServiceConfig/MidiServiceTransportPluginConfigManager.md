---
layout: sdk_reference_page
title: MidiServiceTransportPluginConfigManager
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Class used to update information in the service. Typically not used directly by apps.
---

The `MidiServiceTransportPluginConfigManager` class contains methods which are typically used only by the client-side configuration components of transports. The JSON in these messages must be understood by the service and known to the service transport to be processed properly.

Unless you are authoring a new transport, it is not recommended that you use this class. Instead, known configuration options are provided by strongly-typed classes.

**NOTE: The json config sent to and received from the service is an implementation detail, not a contract, and is subject to change. Do not attempt to manually manipulate or create json to send to the MIDI Service, or manually parse the json return results unless you are creating a transport yourself.**

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `SendUpdate(configUpdate)` | Sends an `IMidiServiceTransportPluginConfig` update to the service to be used by a transport plugin. Returns a `MidiServiceConfigResponse`. |
| `SendUpdate(transportId, fullConfigObject)` | Sends a raw JSON configuration object to the service for the specified transport. Returns a `MidiServiceConfigResponse`. |
| `SendCommand(command)` | Sends a `MidiServiceTransportCommand` to the service. Returns a `MidiServiceConfigResponse`. |
| `SaveUpdate(configUpdate)` | Writes an `IMidiServiceTransportPluginConfig` into the configuration file so it survives a service restart. Returns a `MidiServiceConfigSaveResponse`. |
| `SaveUpdate(transportId, fullConfigObject)` | Writes a raw JSON configuration object into the configuration file for the specified transport. Returns a `MidiServiceConfigSaveResponse`. |
| `QueryCapability(transportId, capabilityQueryKey)` | Returns true when the named transport reports that capability. Use it before offering a feature which not every transport implements. |
| `QueryAllCapabilities(transportId)` | Returns every capability the named transport reports, as a map of key to boolean. |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ConfigFilePath` | The configuration file `SaveUpdate` will write. Empty when no configuration file is registered on this PC. |

## Sending and saving are separate

Sending applies a change to the running service. Saving records it in the configuration file so it comes back after a restart. They are deliberately separate calls, because both halves are useful on their own:

- Sending without saving is how a tool offers a temporary change, such as connecting a device for this session only.
- Saving without sending is how a tool records a change for a device which is not present right now.

A typical tool sends first, and saves only if the send succeeded.

## What can be saved

`SaveUpdate` looks at the shape of the configuration to decide what to do with it, and will not store something which was never meant to be kept.

| Contains | Action |
| --- | --- |
| `create` | Merged into the stored `create` entries |
| `update` | Merged into the matching stored `update` entry, or added when there is none |
| `remove` | Deletes the named entries from the stored `create`. The removal itself is never written, because a removal left in the file would run again on every service start |
| a transport command | Rejected with `ErrorNotPersistable`. A command tells the service to do something now; there is nothing in it to store |

Configurations which only have meaning for the process which created them, such as virtual device creation, are also never saved.

## Merging, not replacing

A saved change is merged into what is already in the file, so a caller only has to supply the part it changed. Changing the name in an endpoint customization leaves that endpoint's stored description and image alone.

Because a merge cannot remove a key, clearing a value means writing it as empty rather than leaving it out.

## Comments in the file

JSON has no comments, so the configuration file uses a `_comment` key, which the service never reads back. `SaveUpdate` writes one into each transport's section naming that transport, so somebody opening the file can tell which GUID is which without looking it up. The name comes from the transport's own metadata rather than a fixed table, so it cannot drift from what is displayed elsewhere.

Individual configuration types may write their own. `MidiNetworkClientConnectConfig` and `MidiBluetoothDeviceConnectConfig` both expose a `Comment` property which is written into the entry they create, so an entry identified only by an address is still readable.

When the file is written, `_comment` is emitted as the first key of the object it describes, wherever it happens to sit in memory. A `_comment` already in the file is preserved by a merge like any other key.

## Concurrency

The file is read, merged and written under a single handle held for the whole operation. The service is never blocked from reading it, another writer waits briefly and then receives `ErrorConfigFileBusy`, and a caller which crashes releases its hold immediately because the lock is the file handle itself.
