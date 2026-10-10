---
layout: kb
title: Minimum System Requirements
audience: everyone
description: Minimum system requirements for Windows MIDI Services
categories:
  - Getting Started
---

Windows MIDI Services will run on the latest supported 64 bit desktop versions (Arm and Intel/AMD) of Windows 11.

The Windows MIDI Services API (`Windows.Devices.Midi2`) needs Windows 11 25H2 or later. Windows MIDI Services on Windows 11 24H2 is an older version that doesn't get fixes, and 24H2 never gets the API. Update to Windows 11 25H2 or later. If you can't update, we recommend [Legacy API mode]({{ site.baseurl }}/kb/how-to-change-api-mode/).

MIDI 2.0 requires updates to the USB stack to support the new class driver assignment as well as to support the new framework used for creating the USB driver. In addition, the API, service, plugins, and apps all have a minimum Windows SDK requirement.

There is no support for the following:
- Older versions of Windows 10 or Windows 11
- 32 bit operating systems
- Xbox, Hololens, IoT Core (full IoT SKUs are supported per above version requirements), Surface Hub
