// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <windows.h>
//#include <winternl.h>

//#pragma warning (disable: 4005)
//#include <ntstatus.h>
//#pragma warning (pop)


#include <iostream>
#include <chrono>
#include <format>


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>

using namespace winrt::Windows::Devices::Enumeration;

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

#include <wrl\module.h>
#include <wrl\event.h>
#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <atlconv.h>
#include <string>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

//#include "SWDevice.h"
#include <initguid.h>
//#include "Devpkey.h"
#include <mmdeviceapi.h>


#include "wstring_util.h"

#include <WindowsMidiServicesVersion.h>


#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

// duplicated here to prevent pulling in all of MidiKSDef, which has a ton of baggage
// Needed by MidiKSCommon from the service API
typedef enum _MidiTransport
{
    MidiTransport_Invalid = 0,
    MidiTransport_StandardByteStream = 1,
    MidiTransport_CyclicByteStream = 2,
    MidiTransport_StandardAndCyclicByteStream = 3, // bitmask combination of standard and cyclic bytestream
    MidiTransport_CyclicUMP = 4,
    MidiTransport_StandardByteStreamAndCyclicUMP = 5, // bitmask combination of standard bytestream and cyclic ump
    MidiTransport_CyclicByteStreamAndCyclicUMP = 6, // bitmask combination of cyclic bytestream and cyclic ump
    MidiTransport_StandardAndCyclicByteStreamAndCyclicUMP = 7 // bitmask combination of all transports
} MidiTransport;

#ifndef KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET
#define STATIC_KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET\
            0xfbffd49e, 0xce26, 0x464a, 0x9d, 0xfc, 0xfe, 0xe4, 0x24, 0x56, 0xc8, 0x1c
DEFINE_GUIDSTRUCT("FBFFD49E-CE26-464A-9DFC-FEE42456C81C", KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET);
#define KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET DEFINE_GUIDNAMED(KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
#endif

#ifndef KSPROPSETID_MidiLoopedStreaming
#define STATIC_KSPROPSETID_MidiLoopedStreaming\
            0x1f306ba6, 0xfd9b, 0x427a, 0xbc, 0xb3, 0x27, 0xcb, 0xcf, 0xe, 0xf, 0x19
DEFINE_GUIDSTRUCT("1F306BA6-FD9B-427A-BCB3-27CBCF0E0F19", KSPROPSETID_MidiLoopedStreaming);
#define KSPROPSETID_MidiLoopedStreaming DEFINE_GUIDNAMED(KSPROPSETID_MidiLoopedStreaming)
#endif

#ifndef KSPROPSETID_MIDI2_ENDPOINT_INFORMATION
#define STATIC_KSPROPSETID_MIDI2_ENDPOINT_INFORMATION\
            0x814552d, 0x2a1e, 0x48fe, 0x9f, 0xbd, 0x70, 0xac, 0x67, 0x91, 0x7, 0x55
DEFINE_GUIDSTRUCT("0814552D-2A1E-48FE-9FBD-70AC67910755", KSPROPSETID_MIDI2_ENDPOINT_INFORMATION);
#define KSPROPSETID_MIDI2_ENDPOINT_INFORMATION DEFINE_GUIDNAMED(KSPROPSETID_MIDI2_ENDPOINT_INFORMATION)

typedef enum {
    KSPROPERTY_MIDI2_NATIVEDATAFORMAT,
    KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,
    KSPROPERTY_MIDI2_SERIAL_NUMBER,
    KSPROPERTY_MIDI2_DEVICE_MANUFACTURER,
    KSPROPERTY_MIDI2_DEVICE_VID,
    KSPROPERTY_MIDI2_DEVICE_PID,
} KSPROPERTY_MIDI2_ENDPOINT_INFORMATION;
#endif


//#include "MidiKS.h"
#include "MidiKSCommon.h"
//#include "MidiKSDef.h"

namespace internal = ::WindowsMidiServicesInternal;


