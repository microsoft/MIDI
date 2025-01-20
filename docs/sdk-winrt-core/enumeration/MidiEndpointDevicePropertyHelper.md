---
layout: api_page
title: MidiEndpointDevicePropertyHelper
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointDevicePropertyHelper

A class with static methods which can be used to get friendly names from the Windows MIDI Services properties returned in a property bag.

## Status

This is an *experimental* type and may change in the future.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Static Methods

| Property | Description |
| --------------- | ----------- |
| `GetMidiPropertyNameFromPropertyKey(fmrid, pid)` | Gets the friendlier name for a MIDI property given its GUID and property Id |
| `GetMidiPropertyNameFromPropertyKey(key)` | Gets the friendlier name for a MIDI property given its WinRT hstring property key |
| `IsMidiPropertyKey(fmtid, pid)` | Returns true if the specified property is a Windows MIDI Services property |
| `IsMidiPropertyKey(key)` | Returns true if the specified property is a Windows MIDI Services property |
| `GetAllMidiProperties` | Returns a map of all Windows MIDI Services property keys |

## IDL

[MidiEndpointNativeDataFormat IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointDevicePropertyHelper.idl)

