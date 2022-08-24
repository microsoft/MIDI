// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

#pragma once

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

// ----------------------------------------------------------------------------
// Message routing support client API
// ----------------------------------------------------------------------------

#include "WindowsMidiServicesEnumeration.h"
using namespace Microsoft::Windows::Midi::Enumeration;

namespace Microsoft::Windows::Midi
{
	enum WINDOWSMIDISERVICES_API MidiRouteType
	{

	};


	class WINDOWSMIDISERVICES_API MidiRouteInformation final
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiRouteInformation(const MidiRouteInformation& info);	// don't copy
	public:
		~MidiRouteInformation();
		const MidiObjectId getId();					// Unique Id of the route. Needed when you want to remove it
		const MidiRouteType getType();				// Type of stream. Mostly used to differentiate unidirectional (like DIN) from bidirectional streams
		const char8_t* getName();					// Name of this stream. May have been changed by the user through config tools.
		const char8_t* getDeviceSuppliedName();		// Endpoint name as supplied by the device plug-in or driver
		const wchar_t* getIconFileName();			// Name, without path, of the image used to represent this specific endpoint
		const wchar_t* getDescription();			// Text description of the stream.

		// TODO: Expose appropriate MIDI CI information per group/channel as negotiated by service
		// For example, bandwidth, protocol, etc.
		// Note that entire API is UMP, so translation to/from byte stream happens
		// either in the driver (example: USB) or in the device/transport plugin

	};



	struct WINDOWSMIDISERVICES_API MidiRouteSource
	{
		MidiObjectId SourceDevice;
		MidiObjectId SourceIncomingStream;
		bool AllIncomingGroups;

		// bitmap of channels for each group. 
		// Group is array (0-based)
		// Channels are bits and are 1-based. Bit 1 high for channel 1, bit 2 high for channel 2... bit 16 for channel 16
		// if AllIncomingGroups is set to true, reads only the first element of this array
		uint16_t IncomingChannelsPerGroup[16];			
	};

	struct WINDOWSMIDISERVICES_API MidiRouteDestination
	{
		MidiObjectId DestinationDevice;
		MidiObjectId DestinationOutgoingStream;
		bool PreserveSourceChannelAndGroup;			// if true, sends the original channel and group through
		MidiGroup ForcedGroup;						// if not PreserveSourceChannelAndGroup, this is the destination group to use
		MidiChannel ForcedChannel;					// same as above, but channel
	};

	enum WINDOWSMIDISERVICES_API MidiRouteSplitNoteType
	{
		RouteSplitNoteType7BitNoteNumber,
		RouteSplitNoteTypePitchSevenNine
	};

	struct WINDOWSMIDISERVICES_API MidiRouteSplitDestination
	{
		uint16_t LowNote;
		uint16_t HighNote;
		MidiRouteSplitNoteType NoteType;
		MidiObjectId DestinationDevice;
		MidiObjectId DestinationOutgoingStream;
		bool PreserveSourceChannelAndGroup;			// if true, sends the original channel and group through
		MidiGroup ForcedGroup;						// if not PreserveSourceChannelAndGroup, this is the destination group to use
		MidiChannel ForcedChannel;					// same as above, but channel
	};

	enum WINDOWSMIDISERVICES_API MidiRoutePolyChainAlgorithm
	{
		MidiRoutePolyChainAlgorithmRoundRobin,
		MidiRoutePolyChainAlgorithmRandom,
		MidiRoutePolyChainAlgorithmRandomWithPriority,
	};

	struct WINDOWSMIDISERVICES_API MidiRoutePolyChainDestination
	{
		uint16_t Priority;				// lower has more priority than higher
		uint16_t MaxVoices;
		MidiObjectId DestinationDevice;
		MidiObjectId DestinationOutgoingStream;
		bool PreserveSourceChannelAndGroup;			// if true, sends the original channel and group through
		MidiGroup ForcedGroup;						// if not PreserveSourceChannelAndGroup, this is the destination group to use
		MidiChannel ForcedChannel;					// same as above, but channel
	};

	enum WINDOWSMIDISERVICES_API MidiRouteCreateResultErrorDetail
	{
		MidiRouteCreateResultErrorInvalidSourceDevice = 10,
		MidiRouteCreateResultErrorInvalidDestinationDevice = 11,
		MidiRouteCreateResultErrorInvalidSourceStream = 20,
		MidiRouteCreateResultErrorInvalidDestinationStream = 21,
		MidiRouteCreateResultErrorAlreadyExists = 30,
		MidiRouteCreateResultErrorCircularRouteDetected = 40,
		MidiRouteCreateResultErrorCommunication = 999,
		MidiRouteCreateResultErrorOther = 1000
	};

	struct WINDOWSMIDISERVICES_API MidiRouteCreateResult
	{
		bool Success;
		MidiRouteCreateResultErrorDetail ErrorDetail;	// Additional error information
		MidiRouteInformation* RouteInformation;
	};

	// TODO: This could be in the device enumeration class. Revisit.
	class WINDOWSMIDISERVICES_API MidiRouter
	{
	private:
		struct impl;
		impl* _pimpl;

	public:
		MidiRouteCreateResult CreateStraightThroughRoute(
			const MidiRouteSource& source,
			const MidiRouteDestination& destination,
			const bool persistent);


		MidiRouteCreateResult CreateSplitPointRoute(
			const MidiRouteSource& source,
			const MidiRouteSplitDestination* destinations,
			const int destinationCount,
			const bool persistent);

		MidiRouteCreateResult CreatePolyChainRoute(
			const MidiRouteSource& source,
			const MidiRoutePolyChainDestination* destinations,
			const int destinationCount,
			const bool persistent);

		bool RemoveRoute(const MidiObjectId routeId);

		// TODO: Methods to list all routes. Mostly for user visibility / debugging
		// GetAllRoutesFrom(device)
		// GetAllRoutesFrom(device, stream)
		// GetAllRoutesTo(device)
		// GetAllRoutesTo(device, stream)
	};
}



