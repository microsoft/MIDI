---
layout: sdk_reference_page
title: MidiApi
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Class used for API information and startup
---

## Static Methods

| Static Method | Description |
| -------------- | ----------- |
| `EnsureServiceAvailable()` | Returns true if the Windows MIDI Services service is available and running on the system. Call this before attempting to create a session. |
| `GetCurrentlySelectedApiMode()` | Returns the currently selected [`MidiApiMode`]({{ site.baseurl }}/sdk-reference/MidiApiModeEnum/) for the system. |
