---
layout: kb
title: Required registry entries in Drivers32 for Windows MIDI Services
description: The drivers32 location of the registry is commonly messed up by uninstallers and third-party tools. This article explains what is required.
audience: everyone
---

The midi...midi9 entries in the Drivers32 location in the registry come up often, especially in the context of certain brands of drivers, like Korg.

Location for 64-bit apps
`Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32`

Location for 32-bit apps
`Computer\HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\Drivers32`

This has led to the idea that Windows only supports 10 MIDI devices. In reality, it's only 10 drivers which require their entries in the registry.

## Tools can break this registry location

Some tools, driver uninstallers, and entry reordering tools, can break this location in the registry. For example, they can create an incorrect `midi0` entry when the first entry needs to be named `midi` not `midi0`.

With Windows MIDI Services, these tools can also remove the necessary entries which make Windows MIDI Services work. 

## Necessary MIDI Entries in the registry

Windows MIDI Services does not load drivers from the registry. Instead, it relies on the two `midiN` entries and ignores the remainder. You no longer need to ensure other drivers are listed within the 10 entries.

These entries are used in the 32-bit and 64-bit registry locations.

| Entry | Type | Value | Description |
| ----- | ---- | ----- | ----------- |
| `midi`  | String | `wdmaud.drv` | This is required for loading the in-box MIDI synthesizer |
| `midi1` | String |  `wdmaud2.drv` | This sends control of the rest of enumeration to the midisrv service. |

In addition, these values are used only in the 64-bit registry location.

| Entry | Type | Value | Description |
| `MidisrvTransferComplete` | DWORD | `1` | Indicates that AudioEndpointBuilder should not enumerate MIDI devices. Instead, the Windows MIDI Service will handle that. |
| `UseLegacyMidi` | String | 0=use midisrv, 1=use legacy, 2=hybrid (not recommended) | Optional and defaults to 0 if not specified. |

## Tool to fix the registry entries

The MIDI Settings app has a feature on the troubleshooting page which can be used to fix these registry locations.

## You don't need as many third-party drivers now

Windows MIDI Services is now fully multi-client, which was one of the primary reasons to use vendor drivers for otherwise class-compliant MIDI devices. We recommned that you not install third-party drivers unless necessary.
