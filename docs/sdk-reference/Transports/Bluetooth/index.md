---
layout: sdk_namespace_page
title: App SDK Support for Bluetooth MIDI Endpoints
namespace: Windows.Devices.Midi2.Transports.Bluetooth
description: Namespace for Bluetooth Low Energy MIDI device and peripheral management
---

Types for discovering Bluetooth Low Energy MIDI devices, connecting to them, and publishing this PC so other devices can connect to it.

One namespace covers both Bluetooth Low Energy MIDI 1.0 and the draft MIDI 2.0 transport. Which protocol a device speaks is chosen by the service, which prefers MIDI 2.0 whenever a device offers it, and is reported through `MidiBluetoothProtocol`. The only place an application chooses a protocol is when publishing this PC as a peripheral, because a peripheral has to advertise as one or the other.

Devices are identified by `BluetoothDeviceId`: the twelve hex digit Bluetooth address, not a Windows device interface id.
