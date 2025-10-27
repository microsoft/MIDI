---
layout: sdk_reference_page
title: MidiEndpointConnection COM Extensions
namespace: Microsoft.Windows.Devices.Midi2
library: Microsoft.Windows.Devices.Midi2.dll
type: runtimeclass
implements: IUnknown
description: Fast and no-allocation message send/receive for a MidiEndpointConnection
tags: session, connection, endpoint
---
WinRT is designed to work across multiple languages in a type-safe way. As a result, WinRT APIs cannot expose functions which take pointers as parameters, or return pointers as their result.

To provide more efficiency for the C++ and other COM-aware and pointer-friendly languages which use this SDK, and which often already have their own code to validate the integrity of UMPs, we've added a small set of COM extensions which can be used for send and receive of multiple messages. These are primarily designed for use with Digital Audio Workstation apps, and cross-platform plugin / app frameworks using languages like C++ and Delphi.

> IMPORTANT: When using these interfaces, it is essential that you fully release and reset any COM references before shutting down the SDK and unitializing the apartment. Failure to do so may result in crashes when you shut down the SDK or when you uninitialize COM.

## IMidiEndpointConnectionRaw

*This interface is available starting in Release Candidate 1 of the Windows MIDI Services App SDK.*

This interface is implemented by the `MidiEndpointConnection` type. You obtain a reference to this using either the [C++/WinRT `as` helper](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/consume-com#query-a-com-smart-pointer-for-a-different-interface), or by calling `QueryInterface`. The returned type is a COM reference.

```cpp
MidiEndpointConnection connection = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());

auto receiveConnectionExtension = connection.as<IMidiEndpointConnectionRaw>();
```

You must call the `as<>` or `QueryInterface` on the default interface of the `MidiEndpointConnection` type as shown above. Do not call it on secondary interfaces. The interface is not CoCreatable itself, and exists only as an implemention in `MidiEndpointConnection`.

The `IMidiEndpointConnectionRaw` interface is as follows:

```cpp
#define UUID_IMidiEndpointConnectionRaw 8087b303-0519-31d1-31d1-000000000020

[
	object,
	local,
	uuid(UUID_IMidiEndpointConnectionRaw)
]
interface IMidiEndpointConnectionRaw : IUnknown
{
	// transmission limit for a single call
	UINT32 GetSupportedMaxMidiWordsPerTransmission();

	// returns true if the buffer contains only valid UMP lengths
	// between messages and messages+wordcount. It does not 
	// validate anything else about the UMPs.
	BOOL ValidateBufferHasOnlyCompleteUmps(
		UINT32 wordCount,
		UINT32* messages
	);

	// before sending a buffer of messages, the caller is responsible
	// for confirming that the buffer has only complete UMPs, and that
	// the buffer is smaller than or equal to the transmission limit
	HRESULT SendMidiMessagesRaw(
		UINT64 timestamp,
		UINT32 wordCount,
		UINT32* completeMessages
		);

	// Wire up your callback handler. When this is in play, the normal
	// WinRT message received events including those on listeners 
	// associated with the connection, will not fire. This is designed
	// solely to be a super fast and efficient callback. You can only
	// have one callback handler for a given connection.
	HRESULT SetMessagesReceivedCallback(
		IMidiEndpointConnectionMessagesReceivedCallback* messagesReceivedCallback
	);

	// Remove your callback handler and reinstant normal event routing.
	// Do this before adding a new callback handler, or when you are
	// cleaning up your connection.
	HRESULT RemoveMessagesReceivedCallback();
};
```

| Function | Description |
| -------- | ----------- |
| `GetSupportedMaxMidiWordsPerTransmission` | Returns the maximum number of MIDI words which can be sent in a single transmission. This is an alias for the same function on the `MidiEndpointConnection` type, and will return the same value.  |
| `ValidateBufferHasOnlyCompleteUmps` | Helper function you may optionally use to validate that the buffer you're providing contains only whole UMPs. |
| `SendMidiMessagesRaw` | Takes a buffer of whole UMPs (UMPs shall not be split across multiple buffers/transmissions) and sends them to the MIDI service. This is done without any addition validation of UMP completeness, and without any additional allocations. It is up to the caller to ensure the data in the buffer is valid and complete. |
| `SetMessagesReceivedCallback` | Register a callback handler for receiving a buffer of one or more incoming messages. Currently, this is the only way to receive more than one message in a single function call. When the connection has a callback handler attached, it will not fire any message received events, nor will it process any message listeners. |
| `RemoveMessagesReceivedCallback` | Unregister the callback. This must be done before closing and destroying the connection. |

## IMidiEndpointConnectionMessagesReceivedCallback

A single callback type may optionally handle incoming messages from multiple `MidiEndpointConnection` instances. From an SDK standpoint, there is no inherent advantage or disadvantage to having multiple handlers or a single handler as long as your code is thread-safe.

When attaching only to a single `MidiEndpointConnection`, the handler code does not necessarily need to be thread-safe, because it will be called serially on the callback thread.

The type implementing this callback interface must be a COM type.

```cpp
#define UUID_IMidiEndpointConnectionMessagesReceivedCallback 8087b303-0519-31d1-31d1-000000000010

[
	object,
	local,
	uuid(UUID_IMidiEndpointConnectionMessagesReceivedCallback)
]
interface IMidiEndpointConnectionMessagesReceivedCallback : IUnknown
{
	HRESULT MessagesReceived(
		GUID sessionId,         // MidiSession.SessionId
		GUID connectionId,		// MidiEndpointConnection.ConnectionId (not the same as the endpoint's id)
        UINT64 timestamp, 
		UINT32 wordCount,       // count of 32-bit MIDI words
		UINT32* messages        // pointer to 32-bit MIDI words
	);
};
```

| Function | Description |
| -------- | ----------- |
| `MessagesReceived` | Called when one or more messages have been received from the endpoint. How many messages are included in a single call depends largely upon how the endpoint receives and passes them along, and what additional processing is done on the data in the service. 

The session id and connection id are provided for convenience when using a centralized message handler. 

The messages data pointer is valid only for the duration of the call, as it is a pointer into the cross-process memory-mapped buffer shared with the service. Therefore, all data must be copied and not referenced. 

Your handler should return an S_OK HRESULT when it completes. The messages will be invalidated in the cross-process buffer regardless of the return value from this callback. |

> IMPORTANT: These interfaces are provided for speed and efficiency. When a `MidiEndpointConnection` has a registered `IMidiEndpointConnectionMessagesReceivedCallback`, it will completely bypass all other message handling code, including any listeners and event handlers. Therefore, this cannot be combined with, for example, implementing a Virtual Device app, which is implemented as a connection listener.

The C++ headers required to use these interfaces are included in the NuGet package and the vcpkg starting in Release Candidate 1, in the `winmidi` subfolder. The IDL is available on GitHub for languages which can process those files directly. No other implementation files are provided for the COM interface.

```cpp
#include "winmidi/WindowsMidiServicesAppSdkComExtensions.h"
#include "winmidi/WindowsMidiServicesAppSdkComExtensions_i.c"
```

More complete examples [available on Github](https://aka.ms/midirepo)
