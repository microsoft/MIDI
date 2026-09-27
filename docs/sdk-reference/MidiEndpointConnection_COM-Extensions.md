---
layout: sdk_reference_page
title: MidiEndpointConnection COM Extensions
namespace: Windows.Devices.Midi2
type: runtimeclass
implements: IUnknown
idl: WindowsMidiServicesAppSdkComExtensions.idl
idl_url: https://github.com/microsoft/MIDI/tree/main/src/in-box/Client/WinRT/com-extensions-idl/WindowsMidiServicesAppSdkComExtensions.idl
description: Fast and no-allocation message send/receive for a MidiEndpointConnection
tags: session, connection, endpoint
---
WinRT is built to work safely across many languages. Because of that, a WinRT API can't have functions that take or return pointers.

C++ and other languages that can use COM and pointers can often send and receive faster by passing pointers, and many of these applications already have their own code to check that UMPs are complete. For them, we added a small set of COM extensions for sending and receiving several messages at once. They're mainly meant for digital audio workstation (DAW) applications, and for cross-platform plugin and application frameworks written in languages like C++ and Delphi.

> **Important:** Release and reset every COM reference you got from these interfaces before you shut down the WinRT API and uninitialize the COM apartment. If you don't, your application may crash when you shut down the API or uninitialize COM.

## IMidiEndpointConnectionRaw

*This interface is available starting in Release Candidate 1 of the Windows MIDI Services App SDK.*

`MidiEndpointConnection` implements this interface. Get it with the [C++/WinRT `as` helper](https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/consume-com#query-a-com-smart-pointer-for-a-different-interface) or by calling `QueryInterface`. What you get back is a COM reference.

```cpp
MidiEndpointConnection connection = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());

auto receiveConnectionExtension = connection.as<IMidiEndpointConnectionRaw>();
```

Call `as<>` or `QueryInterface` on the `MidiEndpointConnection` object itself, as shown above, not on one of its other interfaces. You can't create this interface on its own with `CoCreateInstance`. It exists only as part of `MidiEndpointConnection`.

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
		[in, annotation("_In_")] UINT32 wordCount,
		[in, annotation("_In_")] UINT32 const* messages
	);

	// before sending a buffer of messages, the caller is responsible
	// for confirming that the buffer has only complete UMPs, and that
	// the buffer is smaller than or equal to the transmission limit
	HRESULT SendMidiMessagesRaw(
		[in, annotation("_In_")] UINT64 timestamp,
		[in, annotation("_In_")] UINT32 wordCount,
		[in, annotation("_In_")] UINT32 const* completeMessages
		);

	// Wire up your callback handler. When this is in play, the normal
	// WinRT message received events including those on listeners 
	// associated with the connection, will not fire. This is designed
	// solely to be a super fast and efficient callback. You can only
	// have one callback handler for a given connection.
	HRESULT SetMessagesReceivedCallback(
		[in, annotation("_In_")] IMidiEndpointConnectionMessagesReceivedCallback* messagesReceivedCallback
	);

	// Remove your callback handler and restore normal event routing.
	// Do this before adding a new callback handler, or when you are
	// cleaning up your connection.
	HRESULT RemoveMessagesReceivedCallback();
};
```

| Function | Description |
| -------- | ----------- |
| `GetSupportedMaxMidiWordsPerTransmission` | Returns the most MIDI words one call can send. It returns the same value as the function with the same name on `MidiEndpointConnection` |
| `ValidateBufferHasOnlyCompleteUmps` | An optional helper that checks that a buffer holds only whole UMPs |
| `SendMidiMessagesRaw` | Sends a buffer of whole UMPs to the MIDI service. Never split one UMP across two buffers or two calls. It doesn't check that the UMPs are complete and doesn't allocate any memory, so it's up to you to make sure the data is valid and complete |
| `SetMessagesReceivedCallback` | Registers a callback that receives a buffer of one or more incoming messages at a time. Right now, this is the only way to receive more than one message in a single call. While a callback is registered, the connection raises no message received events and runs no message listeners. Call it before `Open()`, and only on a connection that has no message processing plugins. See below |
| `RemoveMessagesReceivedCallback` | Removes the callback. You must do this before you close and destroy the connection. Unlike registering, you can do it at any time |

### When SetMessagesReceivedCallback fails

Registering the callback changes how the whole connection receives messages, so the connection accepts it only when it can keep that promise. It returns:

| HRESULT | Meaning |
| ------- | ------- |
| `S_OK` | The callback is registered. |
| `E_INVALIDARG` | The callback pointer you passed was null. |
| `E_ILLEGAL_METHOD_CALL` | The connection is already open. Register the callback first, and then call `Open()`. If you registered it afterward, any messages that arrived in between would have gone to the event path instead and been lost. |
| `E_ILLEGAL_STATE_CHANGE` | The connection has message processing plugins attached. The callback skips all of them, so registering it would quietly turn off work your application already set up. Remove the plugins first, or use plugins instead of the callback. |

The rule is also checked the other way: `MidiEndpointConnection.AddMessageProcessingPlugin` returns `FailedRawCallbackRegistered` when a callback is registered on that connection. See [`MidiMessageProcessingPluginAddResult`]({{ site.baseurl }}/sdk-reference/MidiMessageProcessingPluginAddResultEnum).

### Why a virtual device can't use the messages received callback

This is the question we're asked most often about the COM extensions, so here's the reason.

The two ways of receiving do very different amounts of work.

**The COM extensions callback doesn't touch your data.** A block of incoming messages arrives from the service in one buffer that's shared between the two processes. If the block is within the size limits, it stays together, and you get a pointer to it. Nothing is allocated, copied, or read, and no message is looked at on its own. That's the whole point of the fast path, and it's why it's the right choice for a DAW or a cross-platform framework that already has its own code to read UMPs.

**The WinRT event path works one message at a time.** It goes through the incoming buffer, finds each message, creates a `MidiMessageReceivedEventArgs` for it, copies the message into it, and raises the event. Then it waits for your handler to return before it does the same for the next message. That's quick in practice, but it's not the fastest possible for languages that understand COM and pointers.

**A virtual device needs the one-message-at-a-time path**, because it doesn't just watch. It has to find endpoint discovery and stream configuration requests in the incoming messages, and send the right answers. Then, by default, it removes those requests, so your application doesn't have to filter out protocol messages it never asked for. [`SuppressHandledMessages`]({{ site.baseurl }}/sdk-reference/Transports/Virtual/MidiVirtualDevice) controls that, and it's `true` unless you change it.

So the conflict has a real cause. The callback promises *we won't look at your messages*. A virtual device needs *something to look at every message, and maybe remove some of them*. Both can't be true on one connection. So the API refuses the combination, instead of quietly giving you a virtual device that sends fine but never answers discovery.

**A virtual device can still send through the COM extensions.** Only receiving is limited, so you can still use `SendMidiMessagesRaw`.

**The limit applies only to the device-side connection.** A virtual device has exactly one device-side connection. It belongs to the application that created the device, and it must use the WinRT receive path. It can't be split across two connections, and no other application can open the device side.

The endpoint your virtual device shows to other applications has no such limit. It's a normal endpoint, and any application that connects to it can use the COM extensions callback, the WinRT events, message listeners, or any other way of sending and receiving the API supports. The limit is on the side that has to answer discovery, not on the side that uses the device.

### Sending more data than fits in one call

`SendMidiMessagesRaw` doesn't check the buffer for you. If `wordCount` is more than `GetSupportedMaxMidiWordsPerTransmission`, the call returns a failure `HRESULT` and **nothing is sent**. Because nothing reached the device, it's safe to try again with a smaller buffer.

This comes up most often with System Exclusive. A System Exclusive 7 UMP carries six data bytes, so a 64 KB bulk dump is about 10,900 UMPs, or about 21,800 MIDI words. That will never fit in one call, so your code has to split it up.

When you split a large buffer:

1. Call `GetSupportedMaxMidiWordsPerTransmission` once for each connection, and keep the value with the rest of that connection's state. Don't hard-code it, and don't assume it's the same for every endpoint or every release.
2. Split only between messages. One UMP must never be spread across two calls. The message type in the top four bits of each UMP's first word sets that message's length, and `ValidateBufferHasOnlyCompleteUmps` confirms that the range you're about to send holds only whole messages.
3. Stop as soon as a call returns a failure `HRESULT`. Sending the rest after a failure only makes it harder for the device to recover.
4. Decide ahead of time what to do if only part of the data gets through. The chunks that were accepted have already reached the device. For System Exclusive in particular, a message that was only partly delivered usually means you have to give up and start the whole transfer again.

```cpp
UINT32 maxWords = connectionRaw->GetSupportedMaxMidiWordsPerTransmission();

uint32_t offset{ 0 };

while (offset < totalWords)
{
    // Gather whole messages until the next one would not fit
    uint32_t chunkWords{ 0 };

    while (offset + chunkWords < totalWords)
    {
        auto packetType = MidiMessageHelper::GetPacketTypeFromMessageFirstWord(words[offset + chunkWords]);

        if (packetType == MidiPacketType::UnknownOrInvalid) return;

        // the MidiPacketType value is also the message length in MIDI words
        const uint32_t messageWords = static_cast<uint32_t>(packetType);

        if (chunkWords + messageWords > maxWords) break;

        chunkWords += messageWords;
    }

    if (chunkWords == 0) return;

    if (!connectionRaw->ValidateBufferHasOnlyCompleteUmps(chunkWords, words + offset)) return;

    if (FAILED(connectionRaw->SendMidiMessagesRaw(
        MidiClock::TimestampConstantSendImmediately(), chunkWords, words + offset)))
    {
        // Any earlier chunks have already reached the device. Do not send the rest.
        break;
    }

    offset += chunkWords;
}
```

If you're writing a cross-platform framework or a language projection on top of this API, do this splitting in your own code. Don't make the applications built on it do it. They usually can't reach `GetSupportedMaxMidiWordsPerTransmission` through your layer, so they would have to hard-code a limit, and a hard-coded limit isn't guaranteed to stay correct.

## IMidiEndpointConnectionMessagesReceivedCallback

One callback object can handle incoming messages from several `MidiEndpointConnection` objects. As far as the API is concerned, one handler or many makes no difference, as long as your code is thread-safe.

If the callback is attached to only one `MidiEndpointConnection`, it doesn't need to be thread-safe, because it's called on one thread, one call at a time.

The type that implements this callback interface must be a COM type.

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
		GUID sessionId,               // MidiSession.SessionId
		GUID connectionId,            // MidiEndpointConnection.ConnectionId (not the same as the endpoint's id)
        UINT64 timestamp, 
		UINT32 wordCount,             // count of 32-bit MIDI words
		UINT32 const* messages        // read-only pointer to 32-bit MIDI words
	);
};
```

The connection waits for `MessagesReceived` to return before it hands over more messages, so keep your callback fast.

Applications are usually much faster than devices. But if your callback can't keep up, the incoming message queue can fill up and cause errors. MIDI 2.0 has no speed limit for devices, and USB 3 and network devices, among others, can send a lot of messages in a very short time.

If you need to do slow work with incoming messages, copy them to your own queue and process them on another thread.

| Function | Description |
| -------- | ----------- |
| `MessagesReceived` | Called when one or more messages arrive from the endpoint. How many messages come in one call depends mostly on how the endpoint receives and passes them along, and on what the service does with the data on the way. `messages` is never `nullptr`, and `wordCount` is always at least 1 |

The session id and connection id are there to help when one handler serves several connections.

The `messages` pointer is valid only during the call, because it points into memory shared with the service. Copy any data you want to keep, and don't keep the pointer.

Return `S_OK` when your handler finishes. The messages are removed from the shared buffer whatever your callback returns.

> **Important:** These interfaces are here for speed. When a `MidiEndpointConnection` has a registered `IMidiEndpointConnectionMessagesReceivedCallback`, it skips all other message handling, including every message listener and message received event handler. So you can't combine it with a virtual device, which works as a message listener on the connection. A virtual device can still *send* messages through the COM extensions, though.

The C++ headers you need are in the NuGet package and the vcpkg port, in the `winmidi` folder, starting with Release Candidate 1. The IDL is on GitHub for languages that can read IDL files directly. We don't provide any other files for the COM interfaces.

```cpp
#include "winmidi/WindowsMidiServicesAppSdkComExtensions.h"
#include "winmidi/WindowsMidiServicesAppSdkComExtensions_i.c"
```

More complete examples are [available on GitHub](https://aka.ms/midirepo)

## Samples

* [C++/WinRT com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/com-extensions) shows allocation-free sending and receiving
* [C++/WinRT scheduled-messages-com-extensions](https://github.com/microsoft/MIDI/tree/main/samples/cpp-winrt/scheduled-messages-com-extensions) shows that the timestamp argument to `SendMidiMessagesRaw` schedules a message the same way the WinRT senders do

Only C++ and other languages that can use COM can use the COM extensions, so these samples have no C# versions.
