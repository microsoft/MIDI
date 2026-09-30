---
layout: sdk_reference_page
title: MidiServiceTransportPluginConfigManager
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Class used to update information in the service. Typically not used directly by apps.
---

`MidiServiceTransportPluginConfigManager` sends configuration to the MIDI service, and saves it. The configuration classes that come with each transport are its main users. The transport in the service has to understand the JSON, or it can't use it.

Unless you're writing a new transport, use the methods that take a configuration class, not the ones that take raw JSON. Each transport has configuration classes for the options it knows about.

> **Important:** The JSON that goes to and from the service is an implementation detail, not a contract, and it can change. Don't build or edit this JSON by hand, and don't parse what comes back, unless you're writing a transport yourself.

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `SendUpdate(configUpdate)` | Sends an `IMidiServiceTransportPluginConfig` to the running service. Returns a `MidiServiceConfigResponse` |
| `SendUpdate(transportId, fullConfigObject)` | Sends a raw JSON configuration object to the running service, for this transport. Returns a `MidiServiceConfigResponse` |
| `SendCommand(command)` | Sends a `MidiServiceTransportCommand` to the service. Returns a `MidiServiceConfigResponse` |
| `GetEndpointCustomizations()` | Returns the stored endpoint customizations from every transport that can report them, as `MidiServiceEndpointCustomization` objects. This includes entries that don't match any endpoint on this PC |
| `GetEndpointCustomizations(transportId)` | The same, for one transport |
| `SaveUpdate(configUpdate)` | Saves an `IMidiServiceTransportPluginConfig` to the configuration file, so it comes back after the service restarts. Returns a `MidiServiceConfigSaveResponse` |
| `SaveUpdate(transportId, fullConfigObject)` | Saves a raw JSON configuration object to the configuration file, for this transport. Returns a `MidiServiceConfigSaveResponse` |
| `QueryCapability(transportId, capabilityQueryKey)` | Returns true if the transport says it has this capability. Check it before you offer a feature that not every transport has |
| `QueryAllCapabilities(transportId)` | Returns every capability the transport reports, as a map of key to true or false |
| `EnsureConfigurationFile()` | Registers the default configuration file, and creates it if this PC has none. That way, a PC that has only ever had a transport package installed can still save settings. Does nothing if a file is already registered. Returns a `MidiServiceConfigSaveResponse` |

## Static Properties

| Static Property | Description |
| --------------- | ----------- |
| `ConfigFilePath` | The configuration file `SaveUpdate` writes to. Empty when this PC has no configuration file registered |

## Sending and saving are different steps

Sending changes the running service. Saving writes the change to the configuration file, so it comes back after a restart. They're separate calls on purpose, because each one is useful by itself:

- Send without saving to make a change that lasts only until the service restarts, such as connecting a device just for now.
- Save without sending to record a change for a device that isn't connected right now.

Most tools send first, and save only if the send worked.

## What can be saved

`SaveUpdate` won't store something that was never meant to be kept. Transport commands are rejected with `ErrorNotPersistable`, because a command tells the service to do something now, and there's nothing in it to store. Configurations that only mean something to the app that made them, such as creating a virtual device, are never saved either. A removal, such as `MidiServiceEndpointCustomizationRemovalConfig`, deletes the entry it names and isn't stored itself.

## Merging, not replacing

A saved change is merged with what's already saved, so you only have to supply the part you changed. Changing the name in an endpoint customization leaves that endpoint's stored description and image alone.

A merge can't remove a value. To clear one, write it as empty, instead of leaving it out.

Some configuration classes, such as `MidiNetworkClientConnectConfig` and `MidiBluetoothDeviceConnectConfig`, have a `Comment` property. It's saved with the entry, so an entry known only by an address still makes sense to a person.

## Saving at the same time

Only one program can save at a time. Another program that tries to save waits a short time, and then gets `ErrorConfigFileBusy`, so it can try again. The service can always read the configuration while a save is happening.

> **Important:** Always change the configuration through this class, or the tools that come with Windows MIDI Services. Don't read or edit the configuration file yourself. Its format, name, and location can change in any release.
