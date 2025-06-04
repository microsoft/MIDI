---
layout: kb
title: Minimum System Requirements
audience: everyone
description: Minimum system requirements for Windows MIDI Services
---

Windows MIDI Services will run on the latest supported 64 bit desktop versions (Arm and Intel/AMD) of Windows 11. We are considering support for (after version 1.0 release) the latest supported version of Windows 10.

MIDI 2.0 requires updates to the USB stack to support the new class driver assignment as well as to support the new framework used for creating the USB driver. In addition, the API, service, plugins, and apps all have a minimum Windows SDK requirement of 10.0.20348.

There is no support for the following:
- Older versions of Windows 10 or Windows 11
- 32 bit operating systems
- Xbox, Hololens, IoT Core (full IoT SKUs are supported per above version requirements), Surface Hub

Developers can detect Windows MIDI Services using the sample code provided in the repo, as well as the functions in the SDK bootstrapper.

## Older versions

Installing Windows MIDI Services preview versions on unsupported operating systems will have a number of limitations

- No MIDI 2.0 driver, and no underlying USB stack support for that driver
- No switchover to wdmaud2.drv and so no WinMM support. This requires code in the AudioEndpointBuilder service which is not present on older operating systems.
- No re-plumbing of WinRT MIDI 1.0 to go through the service

So the only thing you'll really be able to do is create native MIDI 2.0 endpoints like app-to-app MIDI or loopback endpoints and then use them from apps which understand how to talk to the Windows MIDI Services SDK.