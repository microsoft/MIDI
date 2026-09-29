---
layout: sdk_reference_page
title: MidiBluetoothOfflineRetentionConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Sets how long a MIDI endpoint outlives its Bluetooth device going offline
---

Implements `IMidiServiceTransportPluginConfig`.

Sets how long a MIDI endpoint stays after its device goes offline, for one device or for the whole transport. `MidiBluetoothOfflineRetention` explains what the values mean and why the choice matters.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothOfflineRetentionConfig(retentionSeconds)` | Sets the default for the whole transport, used by every device that's set to `UseTransportDefault` |
| `MidiBluetoothOfflineRetentionConfig(bluetoothDeviceId, retentionSeconds)` | Sets the value for one device. Pass `UseTransportDefault` to go back to the transport's default |

`retentionSeconds` is a `MidiBluetoothOfflineRetention` value, or a number of seconds greater than zero. The transport-wide version can't be `UseTransportDefault`, because there's nothing above it to fall back to.

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The Bluetooth transport's GUID |
| `ConfigJson` | This setting, as JSON |

## Applying and saving

Send it to apply it now, and save it to keep it after the service restarts. You usually want both:

```cpp
MidiBluetoothOfflineRetentionConfig config{ deviceId, 30 };

MidiServiceTransportPluginConfigManager::SendUpdate(config);
MidiServiceTransportPluginConfigManager::SaveUpdate(config);
```

A device's entry is matched in the configuration by its Bluetooth device id. So setting one device leaves every other device alone, along with whether it's turned on.

Setting a device's retention doesn't connect it, and saving it doesn't make the service connect it when it starts. You can set it for a device that isn't connected, and it takes effect when that device connects.
