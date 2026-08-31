---
layout: sdk_reference_page
title: MidiBluetoothPeripheralClientListConfig
namespace: Windows.Devices.Midi2.Transports.Bluetooth
type: runtimeclass
description: Saves the remembered allow and deny lists to the configuration file
---

Implements `IMidiServiceTransportPluginConfig`.

Writes the remembered allow and deny lists to the configuration file, so decisions made with `MidiBluetoothApprovalScope.Always` survive a service restart. The service applies those decisions immediately but never writes that file itself, which is why this exists as a separate step.

## Constructors

| Constructor | Description |
| -------- | ----------- |
| `MidiBluetoothPeripheralClientListConfig(currentStatus)` | Builds the configuration from a `MidiBluetoothPeripheralStatus`. |

## Properties

| Property | Description |
| -------- | ----------- |
| `TransportId` | The Bluetooth transport's GUID. |
| `ConfigJson` | The configuration file representation of both lists. |

## Why it takes the whole status

Both lists are stored whole rather than merged entry by entry, so this must always carry the complete set. Constructing it from a `MidiBluetoothPeripheralStatus` fetched from the service is the reliable way to do that, because the service is what holds the current lists:

```cpp
auto status = MidiBluetoothTransportManager::GetPeripheralStatus();

MidiBluetoothPeripheralClientListConfig config{ status };

MidiServiceTransportPluginConfigManager::SaveUpdate(config);
```

Building the lists from an application's own copy risks writing back a stale set and dropping a decision made somewhere else in the meantime.
