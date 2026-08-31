---
layout: sdk_reference_page
title: MidiServiceConfigSaveResult
namespace: Windows.Devices.Midi2.ServiceConfig
type: enum
description: Indicates success or failure when saving configuration to the configuration file.
---

Enum which indicates whether a configuration was successfully written to the Windows MIDI Services configuration file, and if not, what went wrong. Every failure here is one which can be reported to the customer and acted upon.

Sending a configuration to the service and saving it to the configuration file are separate operations. See `MidiServiceTransportPluginConfigManager.SaveUpdate` for why.

## Properties

| Property | Value | Description |
| --- | --- | --- |
| `Success` | `0x00000000` | The configuration was saved |
| `ErrorNotPersistable` | `0x00000064` | This kind of configuration is never saved. Transport commands, and configurations which have meaning only for the calling process, are applied but not stored |
| `ErrorConfigJsonNullOrEmpty` | `0x00000258` | The supplied config json is missing |
| `ErrorProcessingConfigJson` | `0x00000259` | There's an error in the config json |
| `ErrorNoConfigFileRegistered` | `0x000002BC` | No configuration file is registered on this PC, so there is nowhere to save to |
| `ErrorConfigFileNotValidJson` | `0x000002BD` | The file on disk is not valid JSON. It is left untouched so its contents can still be recovered |
| `ErrorAccessDenied` | `0x000002BE` | The customer does not have permission to write the configuration file |
| `ErrorConfigFileBusy` | `0x000002BF` | Another program is writing the file. Retrying is reasonable |
| `ErrorWritingConfigFile` | `0x000002C0` | The file could not be written |
| `ErrorVerificationFailed` | `0x000002C1` | The file was written but did not read back as valid JSON, so the previous contents were restored |
| `ErrorUnexpected` | `0x000007D0` | An unexpected error prevented the configuration from being saved |
