// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

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
						const MidiObjectId& sessionId,
						const MidiObjectId& deviceId,
						const MidiObjectId& streamId,
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
		MidiDevice();
	public:
		~MidiDevice();

		const Enumeration::MidiDeviceInformation getInformation();

		const bool getOpenStream(const MidiObjectId& streamId, MidiStream& stream);
		// todo: method to get all open streams. Or maybe just a custom collection type

		// session sets these when you open the device
		const MidiObjectId getParentSessionID();


		MidiStreamOpenResult OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiStreamOpenResult OpenStream(const Enumeration::MidiStreamInformation& streamInformation, const MidiStreamOpenOptions options);

		MidiStreamOpenResult OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiStreamOpenResult OpenStream(const MidiObjectId& streamId, const MidiStreamOpenOptions options);

		// maybe these should live in the stream class instead of here
		bool SendUmp(const Enumeration::MidiStreamInformation& information, const Messages::Ump& message);
		bool SendUmp(const MidiObjectId& streamId, const Messages::Ump& message);

		// close this device connection and any open stream connections
		void Close();

		friend class MidiSession;	// TBD if this is necessary. Want to construct devices only through session class
	};

	enum WINDOWSMIDISERVICES_API MidiDeviceOpenResultErrorDetail
	{
		MidiDeviceOpenErrorAlreadyOpenInSession,
		MidiDeviceOpenErrorServiceTimeout,

		MidiDeviceOpenErrorOther
	};

	struct WINDOWSMIDISERVICES_API MidiDeviceOpenResult
	{
		bool Success;
		MidiDeviceOpenResultErrorDetail ErrorDetail;	// Additional error information
		MidiDevice* Device;								// pointer to device in the internal collection
	};


	// ----------------------------------------------------------------------------
	// Session
	// ----------------------------------------------------------------------------

	struct WINDOWSMIDISERVICES_API MidiSessionCreateOptions
	{

	};

	enum WINDOWSMIDISERVICES_API MidiSessionCreateResultErrorDetail
	{
		MidiSessionCreateErrorOther
	};

	struct WINDOWSMIDISERVICES_API MidiSessionCreateResult
	{
		bool Success;
		MidiSessionCreateResultErrorDetail ErrorDetail;	// Additional error information
		MidiSession* Session;		
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

		const MidiObjectId getId();
		const SYSTEMTIME getCreatedDateTime();

		const wchar_t* getName();
		void UpdateName(const wchar_t* newName);		// this makes a server call


		// TODO: may want to consider changing this to similar pattern as others, with a 
		// result class and an instance of the new session.
		static MidiSessionCreateResult Create(const wchar_t* name, const wchar_t* appName, const MidiSessionCreateOptions& options);
		static MidiSessionCreateResult Create(const wchar_t* name, const wchar_t* appName);
		void Close();

		//const std::vector<MidiDevice> getAllOpenDevices();
		const MidiDevice* GetOpenDevice(const MidiObjectId id);
		MidiDeviceOpenResult OpenDevice(const MidiObjectId& deviceId, const MidiDeviceOpenOptions& options);
		MidiDeviceOpenResult OpenDevice(const MidiObjectId& deviceId);




		// TODO: enumerator for open devices/streams

	};
}