---
layout: api_page
title: MidiEndpointDeviceInformationFilters
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointDeviceInformationFilters Enumeration

When enumerating devices, it is helpful to be able to filter for different types of devices. For example, an application providing diagnostic or development services may want to enumerate the diagnostic loopback endpoints. A Digital Audio Workstation, on the other hand, would only want to enumerate the normal UMP and Byte Stream native endpoints.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Properties

| Property | Value | Description |
| --------------- | ---------- | ----------- |
| `StandardNativeUniversalMidiPacketFormat` | `0x00000001` | Include endpoints which are MIDI UMP endpoints natively. These are typically considered MIDI 2.0 devices even if they only send MIDI 1.0 messages in UMP. |
| `StandardNativeMidi1ByteFormat` | `0x00000002` | Include endpoints which are MIDI 1.0 byte stream endpoints natively. These are converted to UMP internally in Windows MIDI Services. |
| `VirtualDeviceResponder` | `0x00000100` | Include endpoints which are virtual devices. Note that this is the device side of the endpoint, not the side available to other applications. Typically, you would not use this. |
| `DiagnosticLoopback` | `0x00010000` | Use this value only when providing development, test, or diagnostic services for MIDI. |
| `DiagnosticPing` | `0x00020000` | You would not normally include this in an enumeration. This endpoint is internal. |
| `AllStandardEndpoints` | `StandardNativeUniversalMidiPacketFormat` plus `StandardNativeMidi1ByteFormat` | This is the value most applications should use, and is the default. |

## IDL

[MidiEndpointDeviceInformationFilterEnum IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointDeviceInformationFilterEnum.idl)

