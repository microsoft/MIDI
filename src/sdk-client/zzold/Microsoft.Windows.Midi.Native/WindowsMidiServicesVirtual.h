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

#include "WindowsMidiServicesEnumeration.h"

// ----------------------------------------------------------------------------
// Virtual device client API
// ----------------------------------------------------------------------------

using namespace Microsoft::Windows::Midi::Enumeration;

namespace Microsoft::Windows::Midi //::inline v0_1_0_pre
{
	enum WINDOWSMIDISERVICES_API MidiStreamCreateResultErrorDetail
	{
		MidiStreamCreateSuccess = 0,

		MidiStreamCreateErrorUnrecognizedDevice,		// bad device ID
		MidiStreamCreateErrorNotSupported,				// the device/transport doesn't allow creating devices
		MidiStreamCreateErrorCommunication = 999,
		MidiStreamCreateErrorOther = 1000
	};

	struct WINDOWSMIDISERVICES_API MidiStreamCreateResult
	{
		bool Success;
		MidiStreamCreateResultErrorDetail ErrorDetail;	// Additional error information
		MidiStreamInformation* StreamInformation;
	};


	enum WINDOWSMIDISERVICES_API MidiDeviceCreateResultErrorDetail
	{
		MidiDeviceCreateSuccess = 0,

		MidiDeviceCreateErrorUnrecognizedTransport,		// bad transport ID
		MidiDeviceCreateErrorNotSupported,				// the transport doesn't allow creating devices
		MidiDeviceCreateErrorCommunication = 999,
		MidiDeviceCreateErrorOther = 1000
	};

	struct WINDOWSMIDISERVICES_API MidiDeviceCreateResult
	{
		bool Success;
		MidiDeviceCreateResultErrorDetail ErrorDetail;	// Additional error information
		MidiDeviceInformation* DeviceInformation;
	};


	// TODO: This could be in the device enumeration class. Revisit.
	class WINDOWSMIDISERVICES_API MidiVirtualDeviceManager
	{
	private:
		struct impl;
		impl* _pimpl;

		MidiVirtualDeviceManager(MidiObjectId owningSessionId);
	public:
		~MidiVirtualDeviceManager();


		static MidiVirtualDeviceManager* Create(MidiObjectId owningSessionId);


		const MidiDeviceCreateResult AddDevice(
			const MidiObjectId deviceId,
			const MidiObjectId transportId,
			const wchar_t* deviceSuppliedName);

		bool RemoveDevice(const MidiObjectId deviceId);


		const MidiStreamCreateResult AddStream(
			const MidiObjectId parentDeviceId,
			const MidiObjectId streamId,
			MidiStreamType streamType,
			const wchar_t* deviceSuppliedStreamName);

		bool RemoveStream(const MidiObjectId deviceId);
	};
}

