---
layout: sdk_reference_page
title: MidiServiceConfigSaveResponse
namespace: Windows.Devices.Midi2.ServiceConfig
type: runtimeclass
description: Result of saving a configuration to the configuration file.
---

Returned by `MidiServiceTransportPluginConfigManager.SaveUpdate`. Reports whether the configuration reached the Windows MIDI Services configuration file, and where it was written.

`ErrorMessage` is localized and written for the customer, so it can be displayed as-is.

## Properties

| Property | Description |
| --- | --- |
| `Result` | A `MidiServiceConfigSaveResult` value indicating success or the specific failure. |
| `Success` | `true` when `Result` is `Success`. Provided so the common case does not need a comparison. |
| `ErrorMessage` | A localized message suitable for showing to the customer. Empty on success. |
| `ConfigFilePath` | The configuration file which was written, or would have been. Empty when none is registered on this PC. |
| `BackupFilePath` | The backup taken before this save, when one was taken. Empty otherwise. |

## Backups

Before the first save of each day, the previous contents of the configuration file are copied alongside it as `<config file name>.<yyyy-MM-dd>.bak`. Later saves on the same day leave that backup alone, so it holds the state the file started the day in and a bad editing session can be undone.

Backups are never removed automatically. The permissions on the configuration folder do not necessarily allow deleting files, and losing a backup is worse than keeping one.
