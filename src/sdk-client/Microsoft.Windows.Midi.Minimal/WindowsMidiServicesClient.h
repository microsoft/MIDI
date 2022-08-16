// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

// This is the primary header file for the Windows MIDI Services API. API headers include:
// - WindowsMidiServicesClient.h	: Required. This file.
// - WindowsMidiServicesUmp.h		: Required. Base Universal MIDI Packet definitions
// - WindowsMidiServicesMessages.h  : Optional. Strongly-typed messages
// - WindowsMidiServicesUtility.h   : Optional. MIDI bit manipulation and other methods

// How to use the Windows MIDI Services API
// ===============================================================================================
// This has been developed using Visual Studio 2022 and C++ 20.
//
// If you are not able to consume C++ types, or are using an older version of the language spec,
// we recommend using the WinRT projections in the winmd. Those encapsulate the functionality 
// from this C++-specific version and add more cross-language support. 
//
// 1. Call the utility function to check that MIDI services are installed on this PC.
//    - If not installed, use the included functions to prompt the user to install
// 2. Create a new session
//    - you can have more than one session per app. Think of them as projects, 
//      tabs, etc.)
//    - Open devices/endpoints are scoped to the session and so have dedicated 
//      Send/Receive queues per endpoint per session
//    - Terminating the session cleans up all the open resources
// 3. Enumerate devices and endpoints
//    - Enumeration is not scoped to the session, but is global to the process
//    - Be sure to wire up callbacks for change notifications as well
// 4. Using the enumeration information, open one or more devices
// 5. Open one or more endpoints on those devices
// 6. Send/receive messages using UMPs or their strongly-typed derived types
//
// NOTES: 
// 
// MIDI CI
//		The Windows Service handles the MIDI CI discovery and protocol negotiation
//		tasks for the entire graph of connected devices and endpoints. Individual
//		applications do not need to send or handle those messages. 
//		Currently in discussion is how much of the rest of MIDI CI is handled
//		transparently by the OS vs passed along to applications.
// Jitter Reduction timestamps
//		Current thinking: Windows service handles creating outgoing Jitter 
//		reduction timestamps for endpoints which have negotiated (through MIDI
//		CI) to have JR timestamps sent. 
//
//		Incoming jitter reduction timestamps from other devices could be passed
//		through for applications to do with as they	please.
//		This can all result in a lot of incoming MIDI traffic, however (one
//		or more incoming JR timestamps per group, per channel, per endpoint 
//		per	250ms or less multiplied by the number of sessions handling incoming
//		MIDI messages from that endpoint). 
//		So the longer term plan is to automatically handle incoming JR 
//		timestamps and do a	running calculation in the service to quantify the 
//		jitter, and then update a property on the endpoint/group/channel with
//		the current	calculated incoming jitter. That's far less traffic going
//		through	another set of buffers and to the various client apps and sessions
//		which have that endpoint open.
//		Regardless, it is our hope that manufacturers following the	recommendation to
//		negotiate JR timestamps only for a single group when the different 
//		groups/channels would have the same jitter.

#pragma once

#include <Windows.h>
#include <string>
#include <filesystem>
#include <functional>
#include <map>

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

#include "WindowsMidiServicesUmp.h"

// TODO: May need to differentiate this namespace from the WinRT namespace
namespace Microsoft::Windows::Midi
{
	// GUIDs are used for IDs. If porting this to another platform, consider
	// usig something like CrossGuid, taking an API dependency on boost, or
	// simply redefining as necessary
	// https://github.com/graeme-hill/crossguid

	using MidiObjectId = GUID;


	namespace Enumeration
	{
		// ----------------------------------------------------------------------------
		// Enumeration
		// ----------------------------------------------------------------------------

		// examples: USB, BLE, RTP
		class WINDOWSMIDISERVICES_API MidiTransportInformation
		{
		private:
			struct impl;
		public:
			MidiObjectId getId();				// Unique Id of the type of transport. Referenced by the device
			char* getName();					// Name, like BLE, RTP, USB etc.
			char* getLongName();				// Longer name like Bluetooth Low Energy MIDI 1.0
			char* getIconFileName();			// Name, without path, of the image used to represent this type of transport
		};

		// examples: Some synth, some controller
		class WINDOWSMIDISERVICES_API MidiDeviceInformation
		{
		private:
			struct impl;
		public:
			MidiObjectId getId();				// Unique Id of the device. Used in most MIDI messaging
			MidiObjectId getTransportId();		// Uinque Id of the transport used by the device. For displaying appropriate name/icons
			char* getName();					// Device name. May have been changed by the user through config tools
			char* getDeviceSuppliedName();		// Device name as supplied by the device plug-in or driver
			char* getSerial();					// If there's a unique serial number for the device, we track it here.
			char* getIconFileName();			// Name, without path, of the image used to represent this specific device
			char* getDescription();				// user-supplied long text description
		};

		enum WINDOWSMIDISERVICES_API MidiEndpointType
		{
			MidiEndpointTypeOutput = 0,
			MidiEndpointTypeInput = 1,
			MidiEndpointTypeBidirectional = 2
		};

		// for USB-connected single devices, an endpoint is typically synonomous
		// with the device but for devices which provide other endpoints (like a
		// synth with multiple DIN MIDI ports, or other USB or network ports), 
		// device:endpoint relationship is a 1:1 to 1:many relationship
		class WINDOWSMIDISERVICES_API MidiEndpointInformation final
		{
		private:
			struct impl;
		public:
			MidiObjectId getId();				// Unique Id of the endpoint. Used in most MIDI messaging
			MidiObjectId getParentDeviceId();	// Unique Id of the parent device which owns this endpoint.
			MidiEndpointType getEndpointType();	// Type of endpoint. Mostly used to differentiate unidirectional (like DIN) from bidirectional streams
			char* getName();					// Name of this endpoint. May have been changed by the user through config tools.
			char* getDeviceSuppliedName();		// Endpoint name as supplied by the device plug-in or driver
			char* getIconFileName();			// Name, without path, of the image used to represent this specific endpoint
			char* getDescription();				// Text description of the endpoint.
		};

		// ----------------------------------------------------------------------------
		// Enumeration change callbacks / delegates
		// ----------------------------------------
		// In previous versions of Windows MIDI APIs, like WinMM or WinRT MIDI, devices
		// and ports could be added or removed, but generally did not otherwise change. 
		// In this version, and in MIDI 2.0 in general, devices, endpoints, and more
		// can change properties at any time. Those changes may be due to MIDI CI
		// notifications, or user action. 
		// 
		// We encourage developers to track when devices/endpoints are added or removed, 
		// or when properties of those devices/endpoints/etc change. 
		// 
		// The API objects themselves will be automatically updated as a result of 
		// these change notifications.
		// ----------------------------------------------------------------------------

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiTransportInformation& information)> MidiTransportAddedCallback;

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiTransportInformation& information)> MidiTransportRemovedCallback;

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiTransportInformation& oldInformation,
			const MidiTransportInformation& newInformation)> MidiTransportChangedCallback;


		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiDeviceInformation& information)> MidiDeviceAddedCallback;

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiDeviceInformation& information)> MidiDeviceRemovedCallback;

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiDeviceInformation& oldInformation,
			const MidiDeviceInformation& newInformation)> MidiDeviceChangedCallback;


		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiEndpointInformation& information)> MidiEndpointAddedCallback;

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiEndpointInformation& information)> MidiEndpointRemovedCallback;

		typedef WINDOWSMIDISERVICES_API std::function<void(
			const MidiEndpointInformation& oldInformation,
			const MidiEndpointInformation& newInformation)> MidiEndpointChangedCallback;



		// Enumerator class. Responsible for exposing information about every device
		// and endpoint known to the system.
		class WINDOWSMIDISERVICES_API MidiEnumerator final
		{
		private:
			struct impl;

		public:
			void Load();

			const MidiTransportInformation& GetTransportInformation(MidiObjectId transportId);
			const MidiDeviceInformation& GetDeviceInformation(MidiObjectId deviceId);
			const MidiDeviceInformation& GetEndpointInformation(MidiObjectId deviceId, MidiObjectId endpointId);

			// TODO: Provide functions that return all of the transports/etc. for proper enumeration (without exporting STL types)


			void SubscribeToTransportChangeNotifications(const MidiTransportAddedCallback& addedCallback, const MidiTransportRemovedCallback& removedCallback, const MidiTransportChangedCallback& changedCallback);
			void SubscribeToDeviceChangeNotifications(const MidiDeviceAddedCallback& addedCallback, const MidiDeviceRemovedCallback& removedCallback, const MidiDeviceChangedCallback& changedCallback);
			void SubscribeToEndpointChangeNotifications(const MidiEndpointAddedCallback& addedCallback, const MidiEndpointRemovedCallback& removedCallback, const MidiEndpointChangedCallback& changedCallback);
		};

	}


	// ----------------------------------------------------------------------------
	// MIDI Message received callback / delegate
	// ----------------------------------------------------------------------------

	class WINDOWSMIDISERVICES_API MidiMessageReader
	{
	private:
		struct impl;
	public:
		bool eof();
		int GetNextUmpWordCount();		// get the word count for the next message in the queue
		Ump GetNextMessage();			// reads appropriate number of words from the queue
		Ump PeekNextMessage();			// returns the next message but does not remove it
	};
	
	typedef WINDOWSMIDISERVICES_API void(*MidiMessagesReceivedCallback)(
						const MidiObjectId& sessionId,
						const MidiObjectId& deviceId,
						const MidiObjectId& endpointId,
						const MidiMessageReader& reader) ;


	// ----------------------------------------------------------------------------
	// Devices and Endpoints
	// ----------------------
	// Not to be confused with EndpointInformation and DeviceInformation, these
	// are the actual interfaces to the devices and endpoints themselves.
	// An endpoint is conceptually the same as a Port or Port pair in MIDI 1.0,
	// with a big distinction: it supports bidirectional communication for
	// MIDI CI and MIDI 2.0, when it is available.
	// Classic MIDI 1.0 InputPort and OutputPort types can still be used by
	// opening them with the appropriate MidiEndpointOpenOptions value.
	// 
	// Note: Currently, there is no provision in this API for taking a classic 
	// MIDI 1.0 output and input and combining them to be treated as a single
	// bidirectional port. We're investigating putting that in the service itself
	// and allowing the user to do that pairing, should they want to.
	// ----------------------------------------------------------------------------


	enum WINDOWSMIDISERVICES_API MidiEndpointOpenOptions
	{
		OpenOutput = 1,				// MIDI Out
		OpenInput = 2,				// MIDI In, will need to wire up message received handler
		OpenBidirectional = 3		// Both. Will need to wire up message received handler
	};

	class WINDOWSMIDISERVICES_API MidiEndpoint final
	{
	private:
		struct impl;
	public:

		const Enumeration::MidiEndpointInformation getInformation();

		friend class MidiDevice;	// TBD if this is necessary. Want to construct endpoints only through device class

	};

	struct WINDOWSMIDISERVICES_API MidiDeviceOpenOptions 
	{
		// TODO
	};

	class WINDOWSMIDISERVICES_API MidiDevice final
	{
	private:
		struct impl;
	public:
		const Enumeration::MidiDeviceInformation getInformation();
		//const std::vector<MidiEndpoint> getAllOpenEndpoints();
		const MidiEndpoint getOpenEndpoint(const MidiObjectId& endpointId);

		// session sets these when you open the device
		const GUID getParentSessionID();
		void setParentSessionID(const MidiObjectId sessionId);


		MidiEndpoint OpenEndpoint(const Enumeration::MidiEndpointInformation& endpointInformation, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiEndpoint OpenEndpoint(const Enumeration::MidiEndpointInformation& endpointInformation, const MidiEndpointOpenOptions options);

		MidiEndpoint OpenEndpoint(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options, const MidiMessagesReceivedCallback& messagesReceivedCallback);
		MidiEndpoint OpenEndpoint(const MidiObjectId& endpointId, const MidiEndpointOpenOptions options);

		// maybe these should live in the endpoint class instead of here
		bool SendUmp(const Enumeration::MidiEndpointInformation& information, const Ump& message);
		bool SendUmp(const MidiObjectId& endpointId, const Ump& message);

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
	public:
		MidiObjectId GetId();
		SYSTEMTIME GetCreatedDateTime();

		const char* GetName();
		void UpdateName(const char* newName);		// this makes a server call


		static MidiSession Create(const std::string& name, const std::string& appName, const MidiSessionCreateOptions& options);
		static MidiSession Create(const std::string& name, const std::string& appName);

		const std::vector<MidiDevice> getAllOpenDevices();
		const MidiDevice getOpenDevice(MidiObjectId id);


		MidiDevice OpenDevice(const MidiObjectId& deviceId, const MidiDeviceOpenOptions& options);
		MidiDevice OpenDevice(const MidiObjectId& deviceId);

		// TODO: Events/callbacks for message receive

		// TODO: enumerator for open devices/endpoints

	};


	// ----------------------------------------------------------------------------
	// Service-specific information and configuration
	// ----------------------------------------------------------------------------


	enum WINDOWSMIDISERVICES_API MidiServicePingResponse
	{
		MidiServicePingPong,
		MidiServicePingTimeOut,
		MidiServicePingError
	};


	struct WINDOWSMIDISERVICES_API MidiServicesComponentVersion
	{
		int32_t Major;
		int32_t Minor;
		int32_t Build;
		int32_t Revision;
	};



	class WINDOWSMIDISERVICES_API MidiService final
	{
	public:

		static MidiServicesComponentVersion getClientApiVersion;

		// returns true if the Windows MIDI Services service is installed
		static bool getIsInstalled();

		// returns true if the SCM reports that the services are running
		// It doesn't tell you if it's functioning properly
		static bool getIsRunning();

		// start the MIDI Service if it's not running. Should be unnecessary in most cases
		static bool Start();		

		// pings the service. This will tell you if it is processing messages.
		static MidiServicePingResponse Ping(MidiServicesComponentVersion& serverVersionReported);

		static const char* getServicesInstallerUri();

		// Returns the full file name, including the path, for an icon. Useful
		// for apps which want to show the icon in the app
		static const char* BuildFullPathDeviceIconFilename(const char* iconFileName);
		static const char* BuildFullPathEndpointIconFilename(const char* iconFileName);
		static const char* BuildFullPathTransportIconFilename(const char* iconFileName);
	};
}