---
layout: page
title: Type Initialization Error at Runtime
parent: Troubleshooting
---

# Troubleshooting

## Errors


### Type Initializer Error

If you receive the error "The type initializer for `Windows.Devices.Midi2.<some class>` threw an exception", in the console app in particular, it usually means that the Windows MIDI Services API is not properly registered on the system. This can happen during development if you do run the installer which puts the appropriate activation entries into the registry.

This should never appear in production when Windows MIDI Services is in the box, unless you are using a development build for which the newer types have not been properly registered.

