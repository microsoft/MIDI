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
#include <winnt.h>		// for flags operator macro

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include "WindowsMidiServicesUmp.h"
#include "WindowsMidiServicesEnumeration.h"


// TODO: May need to differentiate this namespace from the WinRT namespace
namespace Microsoft::Windows::Midi //::inline v0_1_0_pre
{
	// ----------------------------------------------------------------------------
	// MIDI Message received callback / delegate
	// ----------------------------------------------------------------------------

	class WINDOWSMIDISERVICES_API MidiMessageReader
	{
	private:
		struct implMidiMessageReader;
		implMidiMessageReader* _pimpl;

		MidiMessageReader(const MidiMessageReader& info) { _pimpl = nullptr; }	// don't copy
		MidiMessageReader();
	public:
		~MidiMessageReader();

		bool eof();
		int GetNextUmpWordCount();			// get the word count for the next message in the queue

		// reads a message from the queue. If valdate == true, will peek first to validate
		// that the next message really is a Ump32, and fail without a read if the message
		// type doesn't match the method used.
		bool ReadUmp32(Messages::Ump32& message, bool validate = true);
		bool ReadUmp64(Messages::Ump64& message, bool validate = true);
		bool ReadUmp96(Messages::Ump96& message, bool validate = true);
		bool ReadUmp128(Messages::Ump128& message, bool validate = true);

		// requires that a buffer large enough for 4 32-bit words is supplied
		// returns the number of words actually written to the buffer. The results
		// can be used to initialize a UMP. Returns 0 on failure, including an
		// incomplete next message in the queue.
		uint8_t ReadNextSingleUmp(const MidiWord32* buffer);

		friend class MidiStream;
	};

	typedef WINDOWSMIDISERVICES_API void(*MidiMessagesReceivedCallback)(
		const MidiObjectId& sessionId,
		const MidiObjectId& deviceId,
		const MidiObjectId& streamId,
		const MidiMessageReader& reader);




	// ----------------------------------------------------------------------------
	// Devices and Endpoints
	// ----------------------
	// Not to be confused with EndpointInformation and DeviceInformation, these
	// are the actual interfaces to the devices and endpoints themselves.
	// ----------------------------------------------------------------------------


	// TODO: May need to prepend a receive timestamp to incoming
	// messages, adding to the overall size. This is not MIDI protocol, but is
	// internal implementation. second or third most important request from
	// app devs is service-side scheduling of message sending. Maybe include this
	// in the options when opening so we can configure the message queue to include
	// the timestamps


	enum WINDOWSMIDISERVICES_API MidiEndpointOpenOptions
	{
		MidiEndpointOpenOutput = 1,				// MIDI Out
		MidiEndpointOpenInput = 2,				// MIDI In, will need to wire up message received handler
		MidiEndpointOpenBidirectional = 3,		// Both. Will need to wire up message received handler

		MidiEndpointOpenExclusive = 4,			// TBD if we also include Exclusive here.

		MidiEndpointOpenUseSendTimestamps = 8	// this sets up a completely different message queue
	};
	DEFINE_ENUM_FLAG_OPERATORS(MidiEndpointOpenOptions)

	enum WINDOWSMIDISERVICES_API MidiEndpointOpenResultErrorDetail
	{
		MidiEndpointOpenResultErrorNone = 0,

		MidiEndpointOpenErrorAlreadyOpenInSession,
		MidiEndpointOpenErrorExclusiveElsewhere,
		MidiEndpointOpenErrorServiceTimeout,

		MidiEndpointOpenErrorOther
	};

	// timestamp for sending scheduled messages. This is an internal
	// Windows thing, not a MIDI protocol thing.
	struct WINDOWSMIDISERVICES_API MidiMessageTimestamp
	{
		uint64_t Timestamp;							// when to send this message

		const uint64_t Immediate = 0;				// constant for any message that should be sent now
		static const uint64_t getCurrent();				// system call to get the current counter value
		static const uint64_t getFrequency();				// frequency of counts in khz. Won't change until reboot.
	};

	// Info on hardware timers and QPC in Windows: 
	// https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps#hardware-timer-info



	// Change Note:
	// ------------------------
	// Based on conversation with Andrew Mee on Aug 29, 2022, we revisited the idea of 
	// streams vs endpoints. New understanding: Endpoint = Source or destination of MIDI
	// messages. HOWEVER, that is not the same as a MIDI port, as a group/function block
	// can be/is an Endpoint. You won't discover these through PnP, but only through the
	// protocol. Therefore, PnP / driver will need to enumerate devices connected to the
	// PC (or virtual, or RTP, etc.) but the individual Endpoints (port pairs, or however
	// you want to think of them) will only be enumerated through MIDI messages.


	// A MIDI Endpoint is where MIDI messages are sent to or received from. It is not exactly
	// 1:1 with a port in MIDI 1.0. Examples:
	// - A MIDI 1.0 device with 16 virtual cables (8 in, 8 out) could have:
	//		- 16 unidirectional endpoints
	//		- Or any number (up to 8 in this case) bi-directional Endpoints (user-paired in/out ports)
	// - A MIDI 2.0 device with 5 defined function blocks in the stream will have 5 Endpoints
	// Notes:
	// - Endpoints aren't necessarily PnP enumerated. they require MIDI CI calls to get groups/functions
	// - EndpointInformation objects will contain additional data that the user supplies via the settings tools


	// NOTE: A case can be made for eliminating the MidiEndpoint class and having the Device be the only
	// interface. An endpoint would just be informationational though EndpointInformation. What makes this
	// worth considering is the ability for a connected device to rearrange its group/function blocks at
	// runtime, which would then invalidate the endpoint classes being used here.

	// NOTE: Aug 30, 2022: The new Function Block spec really does change this.

	class WINDOWSMIDISERVICES_API MidiEndpoint
	{
	protected:
		struct implMidiEndpoint;
		implMidiEndpoint* _pimpl;

		//MidiStream(const MidiStream& info);				// don't copy
		MidiEndpoint(MidiObjectId endpointId, MidiObjectId parentDeviceId, MidiEndpointOpenOptions options);
	public:
		~MidiEndpoint();
		void Close();

		const MidiObjectId getEndpointInformationId();
		const MidiObjectId getParentDeviceInformationId();

		// tells the service we need to lock this endpoint to send sysex. This will have
		// some sort of timeout logic in the service to automatically unlock after some
		// number of seconds have passed without any sysex traffic. 
		bool LockForSystemExclusiveUse();

		// unlocks the endpoint for all sessions on the PC. Note: in the management API we'll 
		// also need to provide a way for the settings app to be able to release any sysex lock
		// on any endpoint so the user can unblock in case of error.
		bool ReleaseSystemExclusiveLock();

		// Makes a service call to see if this session, or any other session, locked this endpoint
		bool IsLockedForSystemExclusive();

		// send a UMP with no scheduling. 
		bool SendUmp(const Messages::Ump& message);

		// send a UMP with scheduling. Only works if the endpoint was created with that option
		bool SendUmp(MidiMessageTimestamp sendTimestamp, const Messages::Ump& message);


		friend class MidiDevice;	// Want to construct streams only through device class

		// TODO: Need to go through profile spec to see what this should really look like
		// Note that despite being here, these impact any session with this endpoint
		// open, so may need a notification / synchronization method. Also note that
		// these are more granular than an Endpoint (they go to channel) so recommend
		// always requesting these from the endpoint instead of trying to cache the values
		// here. Also note that we will probably want a way for the user to provide CI 
		// profiles and properties for endpoints in the settings app, for endpoints which
		// don't support that natively. This will provide a lot more flexibility for how
		// the user works with CI-enabled apps on their PC.
		// We (Windows) should provide APIs to interact with profiles and property exchange
		// but otherwise should not get in the way here, or try to strongly type anything,
		// or other than the user overrides, cache anything.
		//bool SetCIProfileOn(todo todo);
		//bool SetCIProfileOff(todo todo);
		//someJSONstuff GetCIPropertyData(todo todo);
		//bool SetCIPropertyData(someJSONstuff);
		//bool SubscribeToPropertyData(foo bar, somecallback);

	};


	struct WINDOWSMIDISERVICES_API MidiEndpointOpenResult
	{
		bool Success = false;
		MidiEndpointOpenResultErrorDetail ErrorDetail = MidiEndpointOpenResultErrorDetail::MidiEndpointOpenResultErrorNone;
		MidiEndpoint* StreamReference;						// pointer to stream in the internal collection
	};


	struct WINDOWSMIDISERVICES_API MidiDeviceOpenOptions
	{
		// TODO
	};

	class WINDOWSMIDISERVICES_API MidiDevice final
	{
	private:
		struct implMidiDevice;
		implMidiDevice* _pimpl;

		MidiDevice(const MidiDevice& info);	// don't copy
		MidiDevice(MidiObjectId deviceId, MidiObjectId& parentSessionId);
	public:
		~MidiDevice();

		const MidiObjectId getDeviceInformationId();

		const bool getOpenedStream(const MidiObjectId& streamId, MidiStream& stream);
		// todo: method to get all open streams. Or maybe just a custom collection type

		// session sets these when you open the device
		const MidiObjectId getParentSessionID();

		MidiEndpointOpenResult OpenStream(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiEndpointOpenResult OpenStream(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options);

		// close this device connection and any open stream connections
		void Close();

		friend class MidiSession;	// TBD if this is necessary. Want to construct devices only through session class
	};

	enum WINDOWSMIDISERVICES_API MidiDeviceOpenResultErrorDetail
	{
		MidiDeviceOpenErrorNone = 0,

		MidiDeviceOpenErrorAlreadyOpenInSession,
		MidiDeviceOpenErrorServiceTimeout,

		MidiDeviceOpenErrorOther
	};

	struct WINDOWSMIDISERVICES_API MidiDeviceOpenResult
	{
		bool Success;
		MidiDeviceOpenResultErrorDetail ErrorDetail = MidiDeviceOpenResultErrorDetail::MidiDeviceOpenErrorNone;	// Additional error information
		MidiDevice* DeviceReference;					// pointer to device in the internal collection
	};


	// ----------------------------------------------------------------------------
	// Session
	// ----------------------------------------------------------------------------

	struct WINDOWSMIDISERVICES_API MidiSessionCreateOptions
	{

	};

	enum WINDOWSMIDISERVICES_API MidiSessionCreateResultErrorDetail
	{
		MidiSessionCreateErrorNone = 0,
		MidiSessionCreateErrorOther
	};

	struct WINDOWSMIDISERVICES_API MidiSessionCreateResult
	{
		bool Success = false;
		MidiSessionCreateResultErrorDetail ErrorDetail = MidiSessionCreateResultErrorDetail::MidiSessionCreateErrorNone;
		MidiSession* Session = nullptr;
	};

	class WINDOWSMIDISERVICES_API MidiSession final
	{
	private:
		struct implMidiSession;
		implMidiSession* _pimpl;

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

		const MidiDevice* GetOpenedDevice(const MidiObjectId id);

		MidiDeviceOpenResult OpenDevice(const MidiObjectId& deviceId, const MidiDeviceOpenOptions& options);
		MidiDeviceOpenResult OpenDevice(const MidiObjectId& deviceId);

		// TODO: enumerator for open devices/streams
		//const std::vector<MidiDevice> getAllOpenedDevices();


	};
}