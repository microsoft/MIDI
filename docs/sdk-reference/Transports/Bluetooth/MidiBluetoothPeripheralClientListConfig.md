---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClientListConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Saves the remembered allow and deny lists to the configuration file
---

Implements `IMidiServiceTransportPluginConfig`.

Saves the remembered allow and deny lists to the configuration, so decisions made with `MidiBluetoothApprovalScope.Always` are kept after the service restarts. The service applies those decisions right away, but never saves them itself. That's why this is a separate step.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothPeripheralClientListConfig(currentStatus)` | Builds the configuration from a `MidiBluetoothPeripheralStatus` |

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The Bluetooth transport's GUID |
| `ConfigJson` | Both lists, as JSON |

## Why it takes the whole status

Both lists are saved whole, not merged one entry at a time, so this must always hold the complete lists. The safe way to do that is to create it from a `MidiBluetoothPeripheralStatus` you just got from the service, because the service holds the current lists:

```cpp
auto status = MidiBluetoothTransportManager::GetPeripheralStatus();

MidiBluetoothPeripheralClientListConfig config{ status };

MidiServiceTransportPluginConfigManager::SaveUpdate(config);
```

If you build the lists from your app's own copy, you might save an old version and lose a decision made somewhere else in the meantime.
