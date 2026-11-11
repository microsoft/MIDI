---
layout: sdk_reference_page
title: MidiBluetoothIncomingTimingConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Sets when incoming messages from a Bluetooth MIDI device are handed to apps
---

Implements `IMidiServiceTransportPluginConfig`.

Sets when incoming messages are handed to apps, for one device or for the whole transport. `MidiBluetoothIncomingTiming` explains what the values mean and why the choice matters.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothIncomingTimingConfig(incomingTiming)` | Sets the default for the whole transport, used by every device that's set to `UseTransportDefault` |
| `MidiBluetoothIncomingTimingConfig(bluetoothDeviceId, incomingTiming)` | Sets the value for one device. Pass `UseTransportDefault` to go back to the transport's default |

`incomingTiming` is a `MidiBluetoothIncomingTiming` value. The transport-wide version can't be `UseTransportDefault`, because there's nothing above it to fall back to.

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The Bluetooth transport's GUID |
| `ConfigJson` | This setting, as JSON |

## Applying and saving

Send it to apply it now, and save it to keep it after the service restarts. You usually want both:

```cpp
MidiBluetoothIncomingTimingConfig config{ deviceId, MidiBluetoothIncomingTiming::AsSoonAsPossible };

MidiServiceTransportPluginConfigManager::SendUpdate(config);
MidiServiceTransportPluginConfigManager::SaveUpdate(config);
```

A device's entry is matched in the configuration by its Bluetooth device id. So setting one device leaves every other device alone, along with whether it's turned on.

Setting a device's incoming timing doesn't connect it, and saving it doesn't make the service connect it when it starts. You can set it for a device that isn't connected, and it takes effect when that device connects.