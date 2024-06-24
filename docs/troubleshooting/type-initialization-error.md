---
layout: page
title: Type Initialization Error at Runtime
parent: Troubleshooting
---

# Type Initializer Error

If you receive the error "The type initializer for `Microsoft.Windows.Devices.Midi2.<some class>` threw an exception" at runtime, in the console app in particular, it usually means that the Windows MIDI Services SDK is not properly registered on the system, or you are running on an unsupported version of Windows.
