---
layout: api_page
title: MidiService
parent: Service
grand_parent: Windows.Devices.Midi2 API
has_children: false
---

# MidiService

## Remarks

The MidiService class contains a number of static functions which enable working with the service outside of a specific session. 

### Service Health

| `IsAvailable()` | Returns true of Windows MIDI Services is available on this PC. Calling this function is typically the first step in using Windows MIDI Services in your application. |
| `PingService(UInt8)` | Send the specified count of ping messages to the ping endpoint and report on the status and time. Return if the responses are not received in an internally calculated timeout period. |
| `PingService(UInt8, UInt32)` | Send the specified count of ping messages to the ping endpoint and report on the status and time. Return if responses are not received in the specified timeout period (milliseconds). |

Pinging the Windows service uses the same mechanism as sending any UMP message. The actual message sent is a prioprietary message. (At the time this was created, there was no standard MIDI 2.0 UMP ping message). The message itself is sent to the diagnostics endpoint in the service, which is implemented like any other transport. Therefore, the speed of the pings here and the success of the ping process is a reasonable indicator of service, cross-process queue, and client API health.

The diagnostic ping endpoint does not understand any other type of message, and should not be used by applications other than through the ping functions here.

The ping does not tell you if a specific transport or device is in a bad state. For example, if a specific USB MIDI device has crashed, this ping message will still work because it is not sent out over USB.

Here's an example of ping responses through the MIDI console app

![MIDI Console Ping](./console-ping.png)

### Reporting

| `GetInstalledTransportPlugins()` | Returns a list of `MidiServiceTransportPluginInformation` representing all service transport plugins (also called Abstractions) |
| `GetInstalledMessageProcessingPlugins()` | Returns a list of `MidiServiceMessageProcessingPluginInformation` objects representing all service message processing plugins (also called Transforms) |
| `GetActiveSessions()` | Returns a list of `MidiSessionInformation` detailing all active Windows MIDI Services sessions on this PC. |

### Loopback Endpoints

| `CreateTemporaryLoopbackEndpoints(Guid, MidiServiceLoopbackEndpointDefinition, MidiServiceLoopbackEndpointDefinition)` | Create a pair of loopback endpoints which will live until removed through the API or the service is restarted. |
| `RemoveTemporaryLoopbackEndpoints(Guid)` | Remove a pair of temporary loopback endpoints when provided their association Id Guid. |

Applications creating endpoints for app-to-app MIDI should generally use the Virtual Device support built into the API. However, applications may need to create lightweight loopback endpoints without the protocol negotiation, MIDI 2.0 discovery process, and lifetime management provided by the Virtual Device support. For those scenarios, we have a simple loopback endpoint type.

Loopback endpoints created by the user and stored in the configuration file will persist after the service is restarted or the PC rebooted. Loopback endpoints created through this API call are temporary, and will disappear if the service is restarted. In both cases, this feature requires that the loopback endpoint transport is installed and enabled.

### Runtime Configuration

| `UpdateTransportPluginConfiguration(IMidiServiceTransportPluginConfiguration)` | Sends an update to the service to be used by a transport plugin ("Abstraction") |
| `UpdateProcessingPluginConfiguration(IMidiServiceMessageProcessingPluginConfiguration)` | Sends an update to the service to be used by a message processing plugin ("Transform")  |

For plugins which support updates at runtime, developers of those plugins should create configuration WinRT types which implement the required configuration interfaces, and create the JSON that is used in the service. In this way, third-party service transport and message processing plugins can be created and configured without changes to the API.

> Note: In version 1 of the API, only transports can be configured at runtime. We're working on enabling configuration of message processing plugins. The API is a no-op.

## See also

[MidiService IDL](https://github.com/microsoft/MIDI/blob/main/src/api/Client/Midi2Client/MidiService.idl)