---
layout: sdk_reference_page
title: MidiBluetoothOfflineRetentionConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Sets how long a MIDI endpoint outlives its Bluetooth device going offline
---

Implements `IMidiServiceTransportPluginConfig`.

Sets how long a MIDI endpoint outlives its device going offline, either for one device or for the transport as a whole. See `MidiBluetoothOfflineRetention` for what the values mean and why the choice matters.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothOfflineRetentionConfig(retentionSeconds)` | Sets the transport-wide default, used by every device set to `UseTransportDefault`. |
| `MidiBluetoothOfflineRetentionConfig(bluetoothDeviceId, retentionSeconds)` | Sets the value for a single device. Pass `UseTransportDefault` to drop the override. |

`retentionSeconds` is a `MidiBluetoothOfflineRetention` value or a positive number of seconds. The transport-wide form cannot be `UseTransportDefault`, because there is nothing above it to defer to.

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The Bluetooth transport's GUID. |
| `ConfigJson` | The configuration file representation of this setting. |

## Applying and saving

Send it to apply it now, save it to keep it across a service restart. Both are usually wanted:

```cpp
MidiBluetoothOfflineRetentionConfig config{ deviceId, 30 };

MidiServiceTransportPluginConfigManager::SendUpdate(config);
MidiServiceTransportPluginConfigManager::SaveUpdate(config);
```

A device entry is merged into the configuration file by its Bluetooth device id, so setting one device leaves every other device, and its enabled state, alone.
