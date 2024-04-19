---
layout: page
title: Minimum Requirements
parent: Troubleshooting
---

# Minimum requirements

Windows MIDI Services will run on the latest supported 64 bit desktop versions (Arm and Inte/AMD) of Windows 11 as well as (after version 1.0 release) the latest supported version of Windows 10.

TODO: Get exact minimum versions once we are in-box.

MIDI 2.0 requires updates to the USB stack to support the new class driver assignment as well as to support the new framework used for creating the USB driver. In addition, the API, service, plugins, and apps all have a minimum Windows SDK requirement of 10.0.20348.

There is no support for the following:
- Older versions of Windows 10 or Windows 11
- 32 bit operating systems
- Xbox, Hololens, IoT Core (full IoT SKUs are supported per above version requirements), Surface Hub

If your application is run on an unsupported operating system which does not have Windows MIDI Services installed, your first call into the API will fail with a type activation/invokation exception because there is no code running behind that metadata and no matching types in the registry.
