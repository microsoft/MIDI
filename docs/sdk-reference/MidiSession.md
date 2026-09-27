---
layout: sdk_reference_page
title: MidiSession
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: Windows.Foundation.IClosable, Windows.Foundation.IStringable
description: The first class you will create when connecting to an endpoint
---

You need a session before you can connect to an endpoint.

An application can have as many sessions open as it needs. For example, it might open one session for each open project, or a browser might open one for each tab. The connections you open through a session last only as long as the session does.

## Properties

| Property | Description |
| -------- | ----------- |
| `SessionId`  | A GUID that identifies the session. The API creates it for you |
| `Name` | The session's name. To change it later, call `UpdateName()`, which also tells the service |
| `IsOpen` | True if this session is open and ready to use |
| `Connections` | A map of every connection created through this session. The key is each connection's `ConnectionId`. Calling `DisconnectEndpointConnection` removes the connection from the map |

## Static Methods

| Static Method | Description |
| ------------- | ----------- |
| `Create(sessionName)` | Creates and returns a new session with this name. Tools that list sessions show the name, so pick one that tells people which application it belongs to |

## Methods

| Method | Description |
| ------ | ----------- |
| `CreateEndpointConnection(endpointDeviceId)` | Creates a connection to the endpoint with this id. The connection isn't open yet: attach your event handlers, and then call `Open()` |
| `CreateEndpointConnection(endpointDeviceId, settings)` | The same, using the [`MidiEndpointConnectionSettings`]({{ site.baseurl }}/sdk-reference/MidiEndpointConnectionSettings/) you pass in |
| `DisconnectEndpointConnection(endpointConnectionId)` | Closes a connection and removes it from `Connections` |
| `UpdateName(newName)` | Changes the session's name, both here and in the MIDI service |
| `Close()` | (From `IClosable`) Closes the session and every connection opened through it |

> **Note:** If you close a `MidiEndpointConnection` yourself through `IClosable` (or `IDisposable` in C#), the session doesn't know, and the connection stays in its `Connections` map. Call the session's `DisconnectEndpointConnection` instead so the two stay in step. That's also why we don't recommend putting `CreateEndpointConnection` calls in a `using` statement.

### Samples

C#
```cs
using (var session = MidiSession.Create("API Sample Session"))
{
    ...
}
```

C++
```cpp
// Initialize the WinRT apartment and the WinRT API runtime first.
// The samples show how.

auto session = MidiSession::Create("API Sample Session");

...

session.Close();
```