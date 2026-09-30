---
layout: sdk_namespace_page
title: WinRT API Support for Bluetooth MIDI Endpoints
namespace: Windows.Devices.Midi2.Transports.Bluetooth
description: Namespace for Bluetooth Low Energy MIDI device and peripheral management
---

Types for discovering Bluetooth Low Energy MIDI devices, connecting to them, and publishing this PC so other devices can connect to it.

One namespace covers both Bluetooth Low Energy MIDI 1.0 and the draft MIDI 2.0 transport. The service picks which protocol to use with each device, and prefers MIDI 2.0 whenever a device offers it. `MidiBluetoothProtocol` tells you which one it picked. The only time an app chooses a protocol is when it publishes this PC as a peripheral, because a peripheral has to advertise one or the other.

In Bluetooth terms, the *central* is the device that looks for others and connects to them, and the *peripheral* is the one that advertises and waits. Usually this PC is the central and your MIDI device is the peripheral. When you publish this PC as a peripheral, it's the other way around: a phone or tablet is the central, and it connects to this PC.

Devices are identified by `BluetoothDeviceId`, which is the device's Bluetooth address as twelve hex digits. It isn't a Windows device interface id.
