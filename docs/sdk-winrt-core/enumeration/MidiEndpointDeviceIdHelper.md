---
layout: api_page
title: MidiEndpointDeviceIdHelper
parent: Endpoint Enumeration
grand_parent: Midi2 core
has_children: false
---

# MidiEndpointDeviceIdHelper

There are parts of the Endpoint Device Id which, for a Windows MIDI Service endpoint, are exactly the same. In cases where you may need to display an id in a list or other constrained space, it can be helpful to have a short form of the id. This class is used to convert between the full (long) form and the short form. 

For example:

- Full id: `\\?\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}`
- Short id: `ksa_9447707571394916916`

Another example:

- Full id: `\\?\swd#midisrv#midiu_loop_b_default_loopback_b#{e7cce071-3c03-423f-88d3-f1045d02552b}`
- Short id: `loop_b_default_loopback_b`

You can see that in both cases, the common information from the beginning, and the interface Id from the end, are both stripped out.

> Note: Functions in Windows MIDI Services outside of this class always require the full id. When using shortened ids in the app, always use `GetFullIdFromShortId(shortEndpointDeviceId)` before passing the id to a function

This class works on Windows MIDI Services UMP endpoints only. It does not work on WinRT or WinMM MIDI 1.0 port Ids.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Static Methods

| Property | Description |
| --------------- | ----------- |
| `GetShortIdFromFullId(fullEndpointDeviceId)` | Returns the short form of the Endpoint Device Id |
| `GetFullIdFromShortId(shortEndpointDeviceId)` | Given a short id, returns the full id. No validation is performed to ensure the id is a valid UMP Endpoint |
| `IsWindowsMidiServicesEndpointDeviceId(fullEndpointDeviceId)` | Returns true if the endpoint device id appear to be a Windows MIDI Services UMP Endpoint Device Id. No actual lookup is performed. |
| `NormalizeFullId(fullEndpointDeviceId)` | Returns the id in normalized form: trimmed and lowercase. |

## IDL

[MidiEndpointDeviceIdHelper IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiEndpointDeviceIdHelper.idl)

