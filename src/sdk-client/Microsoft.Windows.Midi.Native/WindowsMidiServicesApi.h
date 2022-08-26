// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------

// ====================================================================
// PRE-RELEASE VERSION. BREAKING CHANGES LIKELY. NOT FOR PRODUCTION USE.
// For more information, please see https://github.com/microsoft/midi
// ====================================================================

// This is the primary header file for the Windows MIDI Services API. API headers include:
// - WindowsMidiServicesApi.h				: Optional. This file for convenience. References all the others
//											  so you only need to include this one file.
// - WindowsMidiServicesBase.h				: Required. Base simple types and enums
// - WindowsMidiServicesSession.h			: Required. Sessions, Devices, Streams.
// - WindowsMidiServicesEnumeration.h		: Required. Enumerate streams and devices
// - WindowsMidiServicesRouting.h			: Optional. For app-created message routes
// - WindowsMidiServicesVirtual.h			: Optional. For app-created virtual devices/streams
// - WindowsMidiServicesUmp.h				: Required. Base Universal MIDI Packet definitions
// - WindowsMidiServicesMessages.h			: Optional. Strongly-typed messages
// - WindowsMidiServicesUtility.h			: Optional. MIDI bit manipulation and other methods
// - WindowsMidiServicesServiceControl.h	: Optional, but recommended.
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
//        of the service running and check that it is responding.
// 2. Create a new session
//		- you can have more than one session per app. Think of them as projects, 
//        tabs, etc.)
//		- Open devices/streams are scoped to the session and so have dedicated 
//        Send/Receive queues per stream per session
//		- Terminating the session cleans up all the open resources
// 3. Enumerate devices and streams
//		- Enumeration is not scoped to the session, but is global to the system
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
//		  MIDI 1.0 ports tied together as a logical bi-directional stream to support
//		  MIDI CI)
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

#ifdef WINDOWSMIDISERVICES_EXPORTS
#define WINDOWSMIDISERVICES_API __declspec(dllexport)
#else
#define WINDOWSMIDISERVICES_API __declspec(dllimport)
#endif

// This is just a convenience file so you don't need to include all of these
// in each file you have referencing the API. Just make sure you have the 
// include paths set up properly so these files can be found

#include "WindowsMidiServicesBase.h"
#include "WindowsMidiServicesUmp.h"
#include "WindowsMidiServicesMessages.h"
#include "WindowsMidiServicesUtility.h"
#include "WindowsMidiServicesEnumeration.h"
#include "WindowsMidiServicesRouting.h"
#include "WindowsMidiServicesVirtual.h"
#include "WindowsMidiServicesSession.h"
#include "WindowsMidiServicesServiceControl.h"

