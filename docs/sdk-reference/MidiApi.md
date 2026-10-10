---
layout: sdk_reference_page
title: MidiApi
namespace: Windows.Devices.Midi2
type: runtimeclass
description: Class used for API information and startup
---

Use `MidiApi` to check that Windows MIDI Services is ready before your application starts using it.

## Static Methods

| Static Method | Description |
| -------------- | ----------- |
| `EnsureServiceAvailable()` | Connects to the MIDI service, starting it if it isn't already running, and returns true if the service answered. Returns false when the PC is in Legacy API mode, because Windows MIDI Services isn't in use then. Call this before you create a session. Because it can start the service, don't use it just to show whether the service is running |
| `GetCurrentlySelectedApiMode()` | Returns the [`MidiApiMode`]({{ site.baseurl }}/sdk-reference/MidiApiModeEnum/) this PC is set to use |
| `IsSystemProvided()` | Returns true when your app is using the copy of the API that comes with Windows, in the Windows system folder. Returns false when it's using its own copy, such as the one the `Windows.Devices.Midi2` NuGet package puts next to your app. When Windows has its own copy, that's the one your app gets, even if the app ships a copy too. This doesn't check the service; use `EnsureServiceAvailable()` for that |
