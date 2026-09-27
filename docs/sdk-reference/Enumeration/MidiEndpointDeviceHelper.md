---
layout: sdk_reference_page
title: MidiEndpointDeviceHelper
namespace: Windows.Devices.Midi2.Enumeration
type: runtimeclass
description: Utility class for working with Windows MIDI Services endpoint device ids and specification-compliant names
---


Parts of the endpoint device id are the same for every Windows MIDI Services endpoint. When you need to show an id in a list or another small space, a short form of the id helps. This class converts between the full (long) form and the short form.

For example:

- Full id: `\\?\swd#midisrv#midiu_ksa_9447707571394916916#{e7cce071-3c03-423f-88d3-f1045d02552b}`
- Short id: `ksa_9447707571394916916`

Another example:

- Full id: `\\?\swd#midisrv#midiu_loop_b_default_loopback_b#{e7cce071-3c03-423f-88d3-f1045d02552b}`
- Short id: `loop_b_default_loopback_b`

In both cases, the shared text at the start and the interface id at the end are removed.

> **Note:** Every other function in Windows MIDI Services needs the full id. If your application uses short ids, always call `GetFullIdFromShortId(shortEndpointDeviceId)` before you pass an id to a function.

This class works only with Windows MIDI Services UMP endpoints. It doesn't work with WinRT or WinMM MIDI 1.0 port ids.

## Static Methods

| Static Method | Description |
| --------------- | ----------- |
| `GetShortIdFromFullId(fullEndpointDeviceId)` | Returns the short form of the endpoint device id |
| `GetFullIdFromShortId(shortEndpointDeviceId)` | Returns the full id for a short id. It doesn't check that the id belongs to a real UMP endpoint |
| `IsPossibleWindowsMidiServicesEndpointDeviceId(fullEndpointDeviceId)` | Returns true if the id looks like a Windows MIDI Services UMP endpoint device id. It only checks the text, and doesn't look the id up |
| `IsPossibleWindowsMidiServicesLegacyApiPortDeviceId(legacyPortDeviceId)` | Returns true if the id looks like a WinRT or WinMM MIDI 1.0 port id that Windows MIDI Services created. It only checks the text, and doesn't look the id up |
| `NormalizeFullId(fullEndpointDeviceId)` | Returns the id with spaces trimmed from both ends and every letter in lowercase, so ids can be compared |
| `EnsureCompliantUmpEndpointName(endpointName)` | Returns the name, shortened if needed to fit the UMP endpoint name limit in the MIDI 2.0 specification |
| `EnsureCompliantProductInstanceId(productInstanceId)` | Returns the product instance id with any characters that aren't allowed in a device id removed, and shortened if needed to fit the specification limit |

## Name and id limits

The MIDI 2.0 specification sets the limits for UMP endpoint names and product instance ids as **UTF-8 byte counts, not character counts**. A name that looks short enough can still be too long once it's encoded. Accented Latin letters take two bytes each, Chinese, Japanese, and Korean characters take three, and emoji take four. A 40-character name in Chinese, Japanese, or Korean is 120 bytes, well over the 98-byte limit for endpoint names.

`EnsureCompliantUmpEndpointName` counts bytes and never cuts a character in half, so the result is always valid text.

Use these when someone types a name or id, to see what the service will really store before you send it.
