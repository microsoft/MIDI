---
layout: tools_page
title: MIDI Registry Fixer tool Overview
tool: midifixreg
description: All about the midifixreg tool
---

`midifixreg` is a tool used to clean up the registry on a PC with Windows MIDI Services installed, after a third-party installer/uninstaller/fixer corrupts the required `midi` and `midi1` entries.

> IMPORTANT: This is for use only on PCs with Windows MIDI Services installed. For other installations, this may result in removing all your third-party drivers for MIDI devices

Windows MIDI Services requires only two registry entries in `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Drivers32`

| Value | Type | Required Value | Description |
| --- | --- | --- | --- |
| `midi` | REG_SZ | wdmaud.drv | This is the in-box MIDI 1.0 driver entry. Note that some utilities incorrectly create a `midi0` entry. That value name is not valid. |
| `midi1` | REG_SZ | wdmaud2.drv | This is the driver which forwards WinMM calls to the MIDI Service. |

This utility ensures those two entries exist, and removes all other midi0...midinn entries. It does not change other non-MIDI entries.

This app must be run in an Administrator console, not simply by double-clicking it.

## How to run

1. Open an Administrator command prompt / terminal
2. Type `midifixreg` and then hit enter
3. Read and respond to the on-screen prompts.
