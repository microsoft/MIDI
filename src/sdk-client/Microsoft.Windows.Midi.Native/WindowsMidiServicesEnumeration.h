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

// ----------------------------------------------------------------------------
// Enumeration
// ----------------------------------------------------------------------------

namespace Microsoft::Windows::Midi::Enumeration
{
	// GUIDs are used for IDs. If porting this to another platform, consider
	// usig something like CrossGuid, taking an API dependency on boost, or
	// simply redefining as necessary
	// https://github.com/graeme-hill/crossguid


	// examples: USB, BLE, RTP
	class WINDOWSMIDISERVICES_API MidiTransportInformation
	{
	private:
		struct impl;
		impl* _pimpl;

		//explicit MidiTransportInformation();
		MidiTransportInformation(const MidiTransportInformation& info);	// don't copy
	public:
		~MidiTransportInformation();
		const MidiObjectId getId();					// Unique Id of the type of transport. Referenced by the device
		const char8_t* getName();					// Name, like BLE, RTP, USB etc.
		const char8_t* getLongName();				// Longer name like Bluetooth Low Energy MIDI 1.0
		const wchar_t* getIconFileName();			// Name, without path, of the image used to represent this type of transport
	};

	// MIDI Device
	// ----------------------------------
	// MIDI Devices are similar to, but not identical to, devices as described
	// in the MIDI CI and MIDI 2.0 specifications. The device, in this case
	// is whatever is connected to the PC. That may be a USB or BLE-connected
	// device, a virtual device with streams, a network device, etc.
	// In the protocol specs, a device is anything connected to MIDI, which
	// includes any of the 256 possible things connected via groups/channels
	// on a single transport
	class WINDOWSMIDISERVICES_API MidiDeviceInformation
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiDeviceInformation(const MidiDeviceInformation& info);	// don't copy
	public:
		~MidiDeviceInformation();
		MidiObjectId getId();						// Unique Id of the device. Used in most MIDI messaging
		MidiObjectId getTransportId();				// Uinque Id of the transport used by the device. For displaying appropriate name/icons
		const char8_t* getName();					// Device name. May have been changed by the user through config tools
		const char8_t* getDeviceSuppliedName();		// Device name as supplied by the device plug-in or driver
		const char8_t* getSerial();					// If there's a unique serial number for the device, we track it here.
		const wchar_t* getIconFileName();			// Name, without path, of the image used to represent this specific device
		const wchar_t* getDescription();			// user-supplied long text description
	};

	enum WINDOWSMIDISERVICES_API MidiStreamType
	{
		MidiStreamTypeOutput = 0,
		MidiStreamTypeInput = 1,
		MidiStreamTypeBidirectional = 2			// either a negotiated port pair, or a true bidirectional stream
	};

	// MIDI Stream
	// ----------------------------------
	// A stream is what UMPs are sent to and received from. Similar to a 
	// Port in MIDI 1.0
	// 
	// For USB-connected single devices, a stream is often 1:1
	// with the device but for devices which provide other streams (like a
	// synth with multiple DIN MIDI ports, or other USB or network ports), 
	// device:stream relationship is a 1:1 to 1:many relationship
	// In addition to true bi-directional streams (like network, USB, etc.)
	// Endpoints also encapsulate any negotiated bi-directional communications,
	// involving pairing of discrete ports, for purposes of MIDI CI. 
	class WINDOWSMIDISERVICES_API MidiStreamInformation final
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiStreamInformation(const MidiStreamInformation& info);	// don't copy
	public:
		~MidiStreamInformation();
		const MidiObjectId getId();					// Unique Id of the stream. Used in most MIDI messaging
		const MidiObjectId getParentDeviceId();		// Unique Id of the parent device which owns this stream.
		const MidiStreamType getStreamType();		// Type of stream. Mostly used to differentiate unidirectional (like DIN) from bidirectional streams
		const char8_t* getName();					// Name of this stream. May have been changed by the user through config tools.
		const char8_t* getDeviceSuppliedName();		// Endpoint name as supplied by the device plug-in or driver
		const wchar_t* getIconFileName();			// Name, without path, of the image used to represent this specific endpoint
		const wchar_t* getDescription();			// Text description of the stream.

		// TODO: Expose appropriate MIDI CI information per group/channel as negotiated by service
		// For example, bandwidth, protocol, etc.
		// Note that entire API is UMP, so translation to/from byte stream happens
		// either in the driver (example: USB) or in the device/transport plugin

	};

	// ----------------------------------------------------------------------------
	// Enumeration change callbacks / delegates
	// ----------------------------------------
	// In previous versions of Windows MIDI APIs, like WinMM or WinRT MIDI, devices
	// and ports could be added or removed, but generally did not otherwise change. 
	// In this version, and in MIDI 2.0 in general, devices, streams, and more
	// can change properties at any time. Those changes may be due to MIDI CI
	// notifications, or user action. 
	// 
	// We encourage developers to track when devices/streams are added or removed, 
	// or when properties of those devices/streams/etc change. 
	// 
	// The API objects themselves will be automatically updated as a result of 
	// these change notifications.
	// ----------------------------------------------------------------------------

	typedef WINDOWSMIDISERVICES_API void (*MidiTransportAddedCallback)(
		const MidiTransportInformation& information);

	typedef WINDOWSMIDISERVICES_API void(*MidiTransportRemovedCallback)(
		const MidiTransportInformation& information) ;

	typedef WINDOWSMIDISERVICES_API void(*MidiTransportChangedCallback)(
		const MidiTransportInformation& oldInformation,
		const MidiTransportInformation& newInformation) ;


	typedef WINDOWSMIDISERVICES_API void(*MidiDeviceAddedCallback)(
		const MidiDeviceInformation& information) ;

	typedef WINDOWSMIDISERVICES_API void(*MidiDeviceRemovedCallback)(
		const MidiDeviceInformation& information) ;

	typedef WINDOWSMIDISERVICES_API void(*MidiDeviceChangedCallback)(
		const MidiDeviceInformation& oldInformation,
		const MidiDeviceInformation& newInformation) ;


	typedef WINDOWSMIDISERVICES_API void(*MidiStreamAddedCallback)(
		const MidiStreamInformation& information) ;

	typedef WINDOWSMIDISERVICES_API void(*MidiStreamRemovedCallback)(
		const MidiStreamInformation& information) ;

	typedef WINDOWSMIDISERVICES_API void(*MidiStreamChangedCallback)(
		const MidiStreamInformation& oldInformation,
		const MidiStreamInformation& newInformation) ;



	// Enumerator class. Responsible for exposing information about every device
	// and stream known to the system. Service-side, the first time enumeration 
	// happens it causes MIDI CI calls to be made to negotiate properties of 
	// connected devices and stream
	class WINDOWSMIDISERVICES_API MidiEnumerator final
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiEnumerator(const MidiEnumerator& info);	// don't copy
	public:
		~MidiEnumerator();

		void Load();

		const MidiTransportInformation& GetTransportInformation(MidiObjectId transportId);
		const MidiDeviceInformation& GetDeviceInformation(MidiObjectId deviceId);
		const MidiStreamInformation& GetStreamInformation(MidiObjectId deviceId, MidiObjectId streamId);

		// TODO: Provide functions that return all of the transports/etc. for proper enumeration (without exporting STL types)


		void SubscribeToTransportChangeNotifications(const MidiTransportAddedCallback& addedCallback, const MidiTransportRemovedCallback& removedCallback, const MidiTransportChangedCallback& changedCallback);
		void SubscribeToDeviceChangeNotifications(const MidiDeviceAddedCallback& addedCallback, const MidiDeviceRemovedCallback& removedCallback, const MidiDeviceChangedCallback& changedCallback);
		void SubscribeToEndpointChangeNotifications(const MidiStreamAddedCallback& addedCallback, const MidiStreamRemovedCallback& removedCallback, const MidiStreamChangedCallback& changedCallback);
	};

}

