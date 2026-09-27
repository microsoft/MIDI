---
layout: sdk_namespace_page
title: WinRT API Service Configuration Overview
namespace: Windows.Devices.Midi2.ServiceConfig
description: Interface between the WinRT API and the MIDI Service for use by the Settings application
---

This namespace has the types that change how the MIDI service is set up. MIDI Settings, the MIDI Console, and the transport setup apps use them. Most applications don't need them.

A change can be sent to the running service, saved to the configuration so it comes back after a restart, or both. [`MidiServiceTransportPluginConfigManager`]({{ site.baseurl }}/sdk-reference/ServiceConfig/MidiServiceTransportPluginConfigManager/) explains how.

> **Important:** Always change the configuration through these types or the tools that come with Windows MIDI Services. Don't read or edit the configuration file yourself. Its format, name, and location can change in any release.



