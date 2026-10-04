---
layout: sdk_reference_page
title: MidiServiceConfigSaveResult
namespace: Windows.Devices.Midi2.ServiceConfig
type: enum
description: Indicates success or failure when saving configuration to the configuration file.
---

Says whether a configuration was saved to the Windows MIDI Services configuration file, and if not, what went wrong. Each failure is one you can tell people about, and that they can do something about.

Sending a configuration to the service and saving it are separate steps. [`MidiServiceTransportPluginConfigManager`]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager/) explains why.

## Properties

| Property | Value | Description |
| --- | --- | --- |
| `Success` | `0x00000000` | The configuration was saved |
| `ErrorNotPersistable` | `0x00000064` | This kind of configuration is never saved. Transport commands, and configurations that only mean something to the app that made them, are applied but not stored |
| `ErrorEntryNotSaved` | `0x00000065` | The change is to an entry that isn't saved, so there's nothing to apply it to. Save the entry first. Update configs, such as `MidiLoopbackUpdateConfig`, known client lists, such as `MidiNetworkHostKnownClientsConfig`, and remote client speeds, such as `MidiNetworkHostRemoteClientSettingsConfig`, return this for an entry that only exists in the running service |
| `ErrorConfigJsonNullOrEmpty` | `0x00000258` | The configuration JSON is missing |
| `ErrorProcessingConfigJson` | `0x00000259` | There's an error in the configuration JSON |
| `ErrorNoConfigFileRegistered` | `0x000002BC` | This PC has no configuration file registered, so there's nowhere to save it |
| `ErrorConfigFileNotValidJson` | `0x000002BD` | The file on disk isn't valid JSON. It's left as is, so what's in it can still be recovered |
| `ErrorAccessDenied` | `0x000002BE` | The person using the app doesn't have permission to write the configuration file |
| `ErrorConfigFileBusy` | `0x000002BF` | Another program is writing the file. It's fine to try again |
| `ErrorWritingConfigFile` | `0x000002C0` | The file couldn't be written |
| `ErrorVerificationFailed` | `0x000002C1` | The file was written but didn't read back as valid JSON, so the old contents were put back |
| `ErrorUnexpected` | `0x000007D0` | Something unexpected stopped the configuration from being saved |
