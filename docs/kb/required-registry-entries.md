---
layout: kb
title: Required registry entries in Drivers32 for Windows MIDI Services
description: The drivers32 location of the registry is commonly messed up by uninstallers and third-party tools. This article explains what is required.
audience: everyone
categories:
  - Troubleshooting
---

The `midi` through `midi9` entries in the Drivers32 part of the registry come up a lot, especially with certain brands of drivers such as Korg.

Location for 64-bit apps: `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32`

Location for 32-bit apps: `Computer\HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Windows NT\CurrentVersion\Drivers32`

There are only ten of these entries, which is where the idea that Windows supports only ten MIDI devices comes from. It was never ten devices. It's ten *drivers* that need an entry here.

## Tools can break this part of the registry

Some tools, driver uninstallers and "MIDI port reordering" utilities break this location. A common example is creating a `midi0` entry, when the first entry has to be named `midi` with no number.

With Windows MIDI Services, those tools can also remove the entries that make Windows MIDI Services work.

## The entries Windows MIDI Services needs

Windows MIDI Services doesn't load drivers from the registry. It uses the two entries below and ignores the rest, so you no longer have to squeeze other drivers into the ten slots.

These entries are used in both the 32-bit and the 64-bit registry locations.

| Entry | Type | Value | Description |
| ----- | ---- | ----- | ----------- |
| `midi`  | String | `wdmaud.drv` | Required for loading the in-box MIDI synthesizer |
| `midi1` | String |  `wdmaud2.drv` | Hands the rest of enumeration over to the midisrv service |

These two are used only in the 64-bit location.

| Entry | Type | Value | Description |
| ----- | ---- | ----- | ----------- |
| `MidisrvTransferComplete` | DWORD | `1` | Tells AudioEndpointBuilder not to enumerate MIDI devices, because the Windows MIDI service handles that |
| `UseLegacyMidi` | String | 0 = use midisrv, 1 = use legacy, 2 = hybrid (not recommended) | Optional. Defaults to 0 when it isn't there. |

## How to fix these entries

The **Registry** page in the [MIDI Troubleshooting and Repair]({{ site.baseurl }}/tools/miditroubleshooter/) app shows what's actually there, marks anything that's wrong, and repairs it for you. Windows has to be restarted afterwards, because the audio and MIDI services read these values when they start.

## You don't need as many third-party drivers now

Windows MIDI Services is fully multi-client, and that was one of the main reasons to install a vendor driver for a device that's already class-compliant. We recommend not installing third-party drivers unless you need them.
