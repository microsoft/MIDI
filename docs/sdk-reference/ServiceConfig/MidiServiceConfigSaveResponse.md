---
layout: sdk_reference_page
title: MidiServiceConfigSaveResponse
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Result of saving a configuration to the configuration file.
---

Returned by `MidiServiceTransportPluginConfigManager.SaveUpdate` and `EnsureConfigurationFile`. It tells you whether the configuration was saved, and where.

`ErrorMessage` is translated and written for the people using your app, so you can show it as is.

## Properties

| Property | Description |
| --- | --- |
| `Result` | A `MidiServiceConfigSaveResult` that says whether it worked, or what went wrong |
| `Success` | `true` when `Result` is `Success`. It saves you a comparison in the usual case |
| `ErrorMessage` | A translated message you can show to people. Empty if it worked |
| `ConfigFilePath` | The configuration file that was written, or would have been. Empty when this PC has no configuration file registered |
| `BackupFilePath` | The backup made before this save, if one was made. Otherwise empty |

## Backups

Before the first save each day, the configuration file is copied to a backup next to it, named `<config file name>.<yyyy-MM-dd>.bak`. Later saves that day leave the backup alone. So the backup holds the file as it was at the start of the day, before any changes made that day.

Backups are never deleted automatically. The permissions on the configuration folder may not allow deleting files, and losing a backup is worse than keeping an extra one.
