---
layout: api_page
title: MidiService
parent: Service
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiService

The MidiService class contains a number of static functions which enable working with the service outside of a specific session. 

## Static Functions

| Static Function | Description |
|---|---|
| `PingService(pingCount)` | Send one or more ping messages to the ping endpoint and report on the status and time. Return if the responses are not received in a calculated timeout period. |
| `PingService(pingCount, timeoutMilliseconds)` | Send one or more ping messages to the ping endpoint and report on the status and time. Return if responses are not received in the specified timeout period. |
| `GetInstalledTransportPlugins()` | Returns a list of `MidiServiceTransportPluginInformation` representing all service transport plugins (also called Abstractions) |
| `GetInstalledMessageProcessingPlugins()` | Returns a list of `MidiServiceMessageProcessingPluginInformation` objects representing all service message processing plugins (also called Transforms) |
| `GetActiveSessions()` | Returns a list of `MidiSessionInformation` detailing all active Windows MIDI Services sessions on this PC. |
| `UpdateRuntimeConfiguration(configurationUpdate)` | Used by client-side SDK components for some transports and other plugins, and by the MIDI Settings app. The format of the data is dependent upon the target specified in the data. Use with caution. For more information, see the [config JSON documentation](../../../config-json.md) |

## A note on the ping process

Pinging the Windows service uses the same mechanism as sending any UMP message. The actual message sent is a prioprietary message. (At the time this was created, there was no standard MIDI 2.0 UMP ping message). The message itself is sent to the diagnostics endpoint in the service, which is implemented like any other transport. Therefore, the speed of the pings here and the success of the ping process is a reasonable indicator of service, cross-process queue, and client API health.

The diagnostic ping endpoint does not understand any other type of message, and should not be used by applications other than through the ping functions here.

The ping does not tell you if a specific transport or device is in a bad state. For example, if a specific USB MIDI device has crashed, this ping message will still work because it is not sent out over USB.

Here's an example of ping responses through the MIDI console app

```
```

## A note on updating runtime configuration

In order to foster an open plugin ecosystem, we need a way to get settings and configuration for those plugins up to them in the Windows service. The way we've chosen to do that is JSON, because that same JSON, when not transient in nature, can also be saved into the permanent configuration file for the active MIDI setup.

The runtime configuration update should only be used by code which understands exactly what will be done with the data, and can handle the response which is returned. It is not a general API endpoint, but is designed for components which will extend Windows MIDI Services.

## IDL

[MidiService IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiService.idl)
