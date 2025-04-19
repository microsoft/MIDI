---
layout: sdk_reference_page
title: MidiServiceTransportPluginInformation
namespace: Microsoft.Windows.Devices.Midi2.Reporting
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
description: Information about a single transport plugin loaded in the MIDI Service
---

This is primarily used by the MIDI Console and MIDI Settings apps to report on the installed transports, and also in the case of MIDI Settings, to control how the configuration UI is set up.

When creating a new transport, these properties are specified in the service plugin itself, as return values from functions on the implemented `IMidiServiceTransportPluginMetadataProvider` interface

| Property | Description |
| --- | --- |
| `Id` | GUID for the transport |
| `Name` | Name of the transport |
| `TransportCode` | Short code for the transport. For example "KSA" |
| `Description` | Description of the transport |
| `SmallImagePath` | Image for the transport. Not currently used, but will be used in future iterations of the settings app. |
| `Author` | Transport author |
| `Version` | The version of the transport in use. This is for informational use only. |
| `IsRuntimeCreatableByApps` | True if endpoints can be created by apps |
| `IsRuntimeCreatableBySettings` | True if endpoints can be created by the settings app, which will also create appropriate config file entries |
| `IsSystemManaged` | True if this is a system-managed transport, like the diagnostics transport |
| `CanConfigure` | True if this can be configured in the settings app |
