---
layout: kb
title: About the Diagnostics Endpoints Transport
audience: developers
description: Information about the diagnostics transport
---

# MIDI Diagnostic Endpoints

| Property | Value |
| -------- | ----- |
| Transport Id | `{ac9b5417-3fe0-4e62-960f-034ee4235a1a}` |
| Abbreviation | `DIAG` |

(Note: This transport cannot be disabled and is not listed in the registry. Its activation is hard-coded into the Windows service. Settings for this transport are not read from the configuration file and cannot be changed.)

Windows MIDI Services comes with three diagnostic endpoints, two of which are there for application development, testing, and debugging. These will not normally be used by musicians or displayed through music-creation applications.

## Loopbacks A and B

Windows MIDI Services comes with two loopback endpoints which are always present if the Windows service is running. These cannot be turned off by applications or configuration, and so may be relied upon by customer support, unit tests, and more.

The Endpoint Device Ids are available as static members of the `MidiDiagnostics` class

```cpp
winrt::hstring MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId();
winrt::hstring MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId();
```

By default, these endpoints are not returned by enumeration calls, because most applications would not want to present them to the user. However, you can include them in the `MidiEndpointDeviceInformation::FindAll` and `MidiEndpointDeviceWatcher::CreateWatcher` device filters by using the `MidiEndpointDeviceInformationFilters` enum value `DiagnosticLoopback` if your application has a diagnostic need for them.

```cpp
MidiEndpointDeviceInformationFilters::DiagnosticLoopback
```

Diagnostic Loopback Endpoints A and B are cross-wired so that any message sent out on loopback A will come in on loopback B, and any message sent out on B will come in on A. In this way, the loopbacks function as a global app-to-app MIDI implementation for testing.

Note that there is only one instance of each endpoint in the system, so if multiple applications use the loopback, the messages will get mixed together like any other endpoint.

> TIP: The Diagnostic Loopback Endpoints are in place for testing and development only. Applications should not present them to users, or use them for communication outside of testing and debugging. Don't expose the diagnostic loopback endpoints as part of the list of endpoints in a production DAW application.

### Special Timestamp Behavior

Normally, an incoming MIDI message will receive a new timestamp when it first arrives from a remote endpoint. In this way, you know exactly when the Windows service first "saw" the message.

The loopback endpoints are special-cased so that they do not alter the original sent timestamp. The entire message and timestamp, is sent back exactly as it is received. If you send a message to a loopback with a timestamp of 0 (send immediately), it will come back with the same 0 timestamp. Similarly, if you specify an actual timestamp, that same timestamp will come back in the received message. The latter can be helpful in unit testing when you need to correlate a sent message with a received message, or you need to verify that the specific timestamp you sent was actually sent.

If you need the more typical timestamp behavior, you can set up app-to-app MIDI virtual endpoints.

### Metadata Capture

The Loopback endpoints capture Endpoint and Function Block metadata just like any other endpoint. Because of this, you can change the name of the endpoint through in-protocol messages. If you do that, simply change it back later using the same type of message, or with a service restart.

## Ping

The ping endpoint is not normally returned through any enumeration. It is for internal use only, and should not be used by any applications. It recognizes only one type of proprietary message that is not part of the standard MIDI 2.0 UMP Protocol.

Behavior and implementation of the Ping endpoint is subject to change and should not be relied upon by any code outside of the SDK.

## Compatibility

Diagnostics endpoints are available only to applications using the Windows MIDI Services SDK and service.
