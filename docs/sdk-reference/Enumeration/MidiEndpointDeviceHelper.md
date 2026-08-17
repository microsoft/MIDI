---
layout: sdk_reference_page
title: MidiEndpointDeviceHelper
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Utility class for working with Windows MIDI Services endpoint device ids and specification-compliant names
---


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

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `GetShortIdFromFullId(fullEndpointDeviceId)` | Returns the short form of the Endpoint Device Id |
| `GetFullIdFromShortId(shortEndpointDeviceId)` | Given a short id, returns the full id. No validation is performed to ensure the id is a valid UMP Endpoint |
| `IsPossibleWindowsMidiServicesEndpointDeviceId(fullEndpointDeviceId)` | Returns true if the endpoint device id appears to be a Windows MIDI Services UMP Endpoint Device Id. No actual lookup is performed. |
| `IsPossibleWindowsMidiServicesLegacyApiPortDeviceId(legacyPortDeviceId)` | Returns true if the id appears to be a WinRT or WinMM MIDI 1.0 port device id created by Windows MIDI Services. No actual lookup is performed. |
| `NormalizeFullId(fullEndpointDeviceId)` | Returns the id in normalized form: trimmed and lowercase. |
| `EnsureCompliantUmpEndpointName(endpointName)` | Returns the supplied name shortened, if necessary, to fit the UMP Endpoint Name limit in the MIDI 2.0 specification. |
| `EnsureCompliantProductInstanceId(productInstanceId)` | Returns the supplied Product Instance Id with characters which are not valid in a device identifier removed, shortened if necessary to the specification limit. |

## Name and Id Compliance

The MIDI 2.0 specification states its UMP Endpoint Name and Product Instance Id limits as **UTF-8 byte counts, not character counts**. A name which looks comfortably short can still exceed the limit once encoded: accented Latin characters take two bytes each, CJK characters three, and emoji four. A 40-character name using CJK characters is 120 bytes, well over the 98 byte endpoint name limit.

`EnsureCompliantUmpEndpointName` measures in bytes and never cuts a character in half, so the result is always valid text rather than a truncated multi-byte sequence.

Use these when you accept a name or id from a user and want to know what the service will actually store, before you submit it.
