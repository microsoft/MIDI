---
layout: api_page
title: MidiSession
parent: Session
grand_parent: Midi2 core
has_children: false
---

# MidiSession

Before you can connect to an endpoint, you must start a new MIDI session. 

An application may have any number of sessions open. For example, the application may open one session per open project, or one session per tab in the case of a browser. The lifetime of endpoint connections opened through a session are controlled through the session.

## Location

| Namespace | Microsoft.Windows.Devices.Midi2 |
| Library | Microsoft.Windows.Devices.Midi2 |

## Implements

`Windows.Foundation.IClosable`
`Windows.Foundation.IStringable`

## Properties

| `SessionId`  | Generated Id for the session |
| `Name` | Name for this session. To change the name after creating the session, use the `UpdateName()` function. This will update the service |
| `Settings`  | The settings used to create this session |
| `IsOpen` | True if this session is open and ready to use |
| `Connections` | Map of all endpoint connections created through this session. Disconnecting an endpoint using `DisconnectEndpointConnection` will remove the connection from this map. The map key is the generated connection GUID that identifies an instance of an endpoint connection |

## Static Methods

The two static functions are factory-pattern methods for creating a new session.

| `Create(sessionName)` | Create and return a new session with the specified name |

## Methods

| `CreateEndpointConnection(String)` | Create a new connection to the specified endpoint device Id |
| `CreateEndpointConnection(String, Boolean)` | Create a new connection to the specified endpoint device Id, with an option to reconnect if a device is disconnected and then reconnected while the connection object is alive |
| `CreateEndpointConnection(String, Boolean, IMidiEndpointConnectionSettings)` | Create a new connection to the specified endpoint device Id, using the provided reconnect value and endpoint-specific settings |
| `DisconnectEndpointConnection(endpointConnectionId)` | Cleanly disconnect an endpoint connection and remove it from the connection map |
| `UpdateName(newName)` | Update the name of this session locally and in the MIDI Service |

> Note: If you manually close a MidiEndpointConnection using `IClosable` (or `IDisposable`), it will not be removed from the MidiSession's collection of endpoints. Instead, use the `DisconnectEndpointConnection` method of the session to keep both in sync. For that reason, we do not recommend that you wrap the `CreateEndpointConnection` calls in a using statement.

## See also

[MidiSession IDL](https://github.com/microsoft/MIDI/blob/main/src/app-sdk/winrt/MidiSession.idl)

### Sample

```cs
using (var session = MidiSession.CreateSession("API Sample Session"))
{
    ...
}

```

