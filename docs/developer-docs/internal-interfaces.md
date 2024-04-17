---
layout: page
title: Internal Interfaces
parent: Windows Midi Services
has_children: false
---

# Internal Interfaces

There are a number of interfaces which are internal/private to Windows MIDI Services and are not part of the public API contract. But because this is an open source project, you can of course see them in the source code and even cocreate them on Windows.

**These COM interfaces shall not be used by any non-Microsoft-authored applications.** In addition to not providing all the functionality expected by your users, it's likely they will change from revision to revision as we add features. Examples include, but are not limited to, `IMidiBiDi`, `IMidiAbstraction`, `IMidiCallback` `IMidiIn`, `IMidiOut`, `IMidiEndpointManager`, `IMidiTransform`, `IMidiDataTransform`, `IMidiAbstractionConfigurationManager`, `IMidiSessionTracker` and others.

Some of those interfaces may be used if you create various types of service plugins executing within the service process. That use is allowed within the defined plugin framework. See the sample MIDI Abstraction and MIDI Transforms for examples.

For client applications, regardless of type, the only supported way to communicate with the Windows Service, plugins, and drivers, are the WinRT types in the WinRT `Windows.Devices.Midi2` namespace. Use of the internal COM interfaces is not permitted.

> If you find that you need to use these private interfaces for something, talk to us so we can see if there's a supported way to accomplish what you want to do. We want to optimize for the end-user experience, and taking dependencies on unsupported interface contracts will not help accomplish that.
