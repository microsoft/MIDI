// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

// This is the primary header file for the Windows MIDI Services API. API headers include:
// - WindowsMidiServicesSession.h			: Required. This file.
// - WindowsMidiServicesEnumeration.h		: Required. Enumerate streams and devices
// - WindowsMidiServicesUmp.h				: Required. Base Universal MIDI Packet definitions
// - WindowsMidiServicesMessages.h			: Optional. Strongly-typed messages
// - WindowsMidiServicesUtility.h			: Optional. MIDI bit manipulation and other methods
// - WindowsMidiServicesServiceControl.h	: Optional, but recommended.
// - WindowsMidiServicesApi.h				: Convenience file. References everything above.
// 
// How to use the Windows MIDI Services API
// ===============================================================================================
// This has been developed using Visual Studio 2022 and C++ 20.
//
// If you are not able to consume C++ types directly, or are using an older version of the 
// language spec, we recommend using the WinRT projections in the winmd. Those encapsulate the 
// functionality from this C++-specific version and add more cross-language support. 
//
// 1. Optional: Call the Service function to check that MIDI services are installed 
//    on this PC.
//		- If not installed, use the included functions to prompt the user to install
//		- If installed, you may optionally use the ping function to get the version
//        of the service running.
// 2. Create a new session
//		- you can have more than one session per app. Think of them as projects, 
//        tabs, etc.)
//		- Open devices/streams are scoped to the session and so have dedicated 
//        Send/Receive queues per endpoint per session
//		- Terminating the session cleans up all the open resources
// 3. Enumerate devices and streams
//		- Enumeration is not scoped to the session, but is global to the process
//		- Be sure to wire up callbacks for change notifications as well
// 4. Using the returned enumeration information, open one or more devices
//		- A device is what is connected to the PC. Devices expose one or more
//        streams (ports) used to send/receive MIDI messages
//		- Pick a stream type based on your intended use. If you want to schedule
//		  messages to be sent at a specific time, use the scheduled version. The
//		  Scheduled version has more memory and processor overhead vs the non-
//		  scheduled queues.
// 5. Open one or more streams on those devices
//		- Device streams in MIDI 2.0 are bi-directional
//		- Device streams can also be configured as bi-directional (two actual
//		  ports tied together as a logical bi-directional stream)
// 6. Send/receive messages using base UMPs or their strongly-typed derived types
//		- The strongly-typed classes are optional, but helpful
//
// NOTES: 
// 
// Jitter Reduction timestamps
//		Current thinking: Windows service handles creating outgoing Jitter 
//		reduction timestamps for streams which have negotiated (through MIDI
//		CI) to have JR timestamps sent. 
//
//		Incoming jitter reduction timestamps from other devices could be passed
//		through for applications to do with as they	please.
//		This can all result in a lot of incoming MIDI traffic, however (one
//		or more incoming JR timestamps per group / channel / stream 
//		per	250ms or less multiplied by the number of sessions handling incoming
//		MIDI messages from that stream). 
//		So the longer term plan is to automatically handle incoming JR 
//		timestamps and do a	running calculation in the service to quantify the 
//		jitter, and then update a property on the stream/group/channel with
//		the current	calculated incoming jitter. That's far less traffic going
//		through	another set of buffers and to the various client apps and sessions
//		which have that stream open.
//		Regardless, it is our hope that manufacturers following the	recommendation to
//		negotiate JR timestamps only for a single group when the different 
//		groups/channels would have the same jitter.

#pragma once

#include <Windows.h>

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include "WindowsMidiServicesUmp.h"
#include "WindowsMidiServicesEnumeration.h"

// TODO: May need to differentiate this namespace from the WinRT namespace
namespace Microsoft::Windows::Midi
{
	// ----------------------------------------------------------------------------
	// MIDI Message received callback / delegate
	// ----------------------------------------------------------------------------

	class WINDOWSMIDISERVICES_API MidiMessageReader
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiMessageReader(const MidiMessageReader& info);	// don't copy
	public:
		~MidiMessageReader();

		bool eof();
		int GetNextUmpWordCount();			// get the word count for the next message in the queue
		Messages::Ump GetNextMessage();		// reads appropriate number of words from the queue
		Messages::Ump PeekNextMessage();	// returns the next message but does not remove it
	};
	
	typedef WINDOWSMIDISERVICES_API void(*MidiMessagesReceivedCallback)(
						const Enumeration::MidiObjectId& sessionId,
						const Enumeration::MidiObjectId& deviceId,
						const Enumeration::MidiObjectId& streamId,
						const MidiMessageReader& reader) ;


	// ----------------------------------------------------------------------------
	// Devices and Streams
	// ----------------------
	// Not to be confused with StreamInformation and DeviceInformation, these
	// are the actual interfaces to the devices and streams themselves.
	// ----------------------------------------------------------------------------


	// TODO: May need to prepend a send/receive timestamp to incoming and outgoing
	// messages, adding to the overall size. This is not MIDI protocol, but is
	// internal implementation. second or third most important request from
	// app devs is service-side scheduling of message sending. Maybe include this
	// in the options when opening so we can configure the message queue to include
	// the timestamps

	enum WINDOWSMIDISERVICES_API MidiStreamOpenOptions
	{
		MidiStreamOpenOutput = 1,				// MIDI Out
		MidiStreamOpenInput = 2,				// MIDI In, will need to wire up message received handler
		MidiStreamOpenBidirectional = 3,		// Both. Will need to wire up message received handler

		MidiStreamOpenExclusive = 4,			// TBD if we also include Exclusive here.

		MidiStreamOpenUseSendTimestamps = 8
	};


	enum WINDOWSMIDISERVICES_API MidiStreamOpenResultErrorDetail
	{
		MidiStreamOpenErrorAlreadyOpenInSession,
		MidiStreamOpenErrorExclusiveElsewhere,
		MidiStreamOpenErrorServiceTimeout,

		MidiStreamOpenErrorOther
	};

	// timestamp for sending scheduled messages
	struct WINDOWSMIDISERVICES_API MidiMessageTimestamp
	{
		uint64_t Timestamp;							// when to send this message

		const uint64_t Immediate = 0;				// constant for any message that should be sent now
		static const uint64_t getCurrent();				// system call to get the current counter value
		static const uint64_t getFrequency();				// frequency of counts in khz. Won't change until reboot.
	};
	// Info on hardware timers and QPC in Windows: 
	// https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps#hardware-timer-info


	class WINDOWSMIDISERVICES_API MidiStream
	{
	protected:
		struct impl;
		impl* _pimpl;

		MidiStream(const MidiStream& info);				// don't copy
		MidiStream(MidiStreamOpenOptions options);
	public:
		~MidiStream();
		virtual void Close() = 0;

		const virtual Enumeration::MidiStreamInformation getInformation();

		friend class MidiDevice;	// TBD if this is necessary. Want to construct streams only through device class

		// TODO: MIDI CI Profile Support
		// TODO: MIDI CI Property Exchange Support
	};

	// stream which handles messages that are immediately sent
	class WINDOWSMIDISERVICES_API MidiImmediateStream final : MidiStream
	{
	private:
		MidiImmediateStream(const MidiImmediateStream& info);	// don't copy
	public:
		~MidiImmediateStream();
		virtual void Close();

		bool SendUmp(const Messages::Ump& message);

		friend class MidiDevice;	// TBD if this is necessary. Want to construct streams only through device class
	};

	// stream which handles timestamped messages. This uses a different
	// type of queue behind the scenes, so different class
	class WINDOWSMIDISERVICES_API MidiScheduledStream final : MidiStream
	{
	private:
		MidiScheduledStream(const MidiScheduledStream& info);	// don't copy
	public:
		~MidiScheduledStream();
		virtual void Close();

		bool SendUmp(MidiMessageTimestamp sendTimestamp, const Messages::Ump& message);

		friend class MidiDevice;	// TBD if this is necessary. Want to construct streams only through device class
	};

	struct WINDOWSMIDISERVICES_API MidiStreamOpenResult
	{
		bool Success;
		MidiStreamOpenResultErrorDetail ErrorDetail;	// Additional error information
		MidiStream* Stream;								// pointer to stream in the internal collection
	};


	struct WINDOWSMIDISERVICES_API MidiDeviceOpenOptions 
	{
		// TODO
	};

	class WINDOWSMIDISERVICES_API MidiDevice final
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiDevice(const MidiDevice& info);	// don't copy
	public:
		~MidiDevice();

		const Enumeration::MidiDeviceInformation getInformation();
		//const std::vector<MidiStream> getAllOpenStreams();
		const bool getOpenEndpoint(const Enumeration::MidiObjectId& streamId, MidiStream& stream);

		// session sets these when you open the device
		const Enumeration::MidiObjectId getParentSessionID();


		MidiStreamOpenResult OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiStreamOpenResult OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options);

		MidiStreamOpenResult OpenStream(const Enumeration::MidiObjectId& streamId, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiStreamOpenResult OpenStream(const Enumeration::MidiObjectId& streamId, const MidiStreamOpenOptions options);

		// maybe these should live in the stream class instead of here
		bool SendUmp(const Enumeration::MidiStreamInformation& information, const Messages::Ump& message);
		bool SendUmp(const Enumeration::MidiObjectId& streamId, const Messages::Ump& message);

		// close this device connection and any open stream connections
		void Close();

		friend class MidiSession;	// TBD if this is necessary. Want to construct devices only through session class
	};


	// ----------------------------------------------------------------------------
	// Session
	// ----------------------------------------------------------------------------

	struct WINDOWSMIDISERVICES_API MidiSessionCreateOptions
	{

	};

	class WINDOWSMIDISERVICES_API MidiSession final
	{
	private:
		struct impl;
		impl* _pimpl ;

		MidiSession();								// use the Create method to construct objects
		MidiSession(const MidiSession& session);	// not copyable
	public:
		~MidiSession();

		const Enumeration::MidiObjectId getId();
		const SYSTEMTIME getCreatedDateTime();

		const wchar_t* getName();
		void UpdateName(const wchar_t* newName);		// this makes a server call


		static MidiSession Create(const wchar_t* name, const wchar_t* appName, const MidiSessionCreateOptions& options);
		static MidiSession Create(const wchar_t* name, const wchar_t* appName);

		//const std::vector<MidiDevice> getAllOpenDevices();
		const MidiDevice GetOpenDevice(const Enumeration::MidiObjectId id);


		MidiDevice OpenDevice(const Enumeration::MidiObjectId& deviceId, const MidiDeviceOpenOptions& options);
		MidiDevice OpenDevice(const Enumeration::MidiObjectId& deviceId);

		// TODO: enumerator for open devices/streams

	};


}