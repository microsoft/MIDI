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


#include <wrl\module.h>
#include <wrl\event.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <mmeapi.h>

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


namespace internal = ::WindowsMidiServicesInternal;



#define MIDI_NOTEOFF                0x80    // MIDI_NOTEOFF
#define MIDI_NOTEON                 0x90    // MIDI_NOTEON
#define MIDI_POLYAFTERTOUCH         0xa0    // MIDI_POLYAFTERTOUCH
#define MIDI_CONTROLCHANGE          0xb0    // MIDI_CONTROLCHANGE
#define MIDI_PROGRAMCHANGE          0xc0    // MIDI_PROGRAMCHANGE
#define MIDI_MONOAFTERTOUCH         0xd0    // MIDI_MONOAFTERTOUCH  (channel pressure)
#define MIDI_PITCHBEND              0xe0    // MIDI_PITCHBEND
#define MIDI_SYSEX                  0xf0    // MIDI_SYSEX
#define MIDI_TIMECODE               0xf1    // MIDI_TIMECODE
#define MIDI_SONGPOSITIONPOINTER    0xf2    // MIDI_SONGPOSITIONPOINTER
#define MIDI_SONGSELECT             0xf3    // MIDI_SONGSELECT
#define MIDI_TUNEREQUEST            0xf6    // MIDI_TUNEREQUEST
#define MIDI_EOX                    0xf7    // MIDI_EOX
#define MIDI_TIMINGCLOCK            0xf8    // MIDI_TIMINGCLOCK
#define MIDI_START                  0xfa    // MIDI_START
#define MIDI_CONTINUE               0xfb    // MIDI_CONTINUE
#define MIDI_STOP                   0xfc    // MIDI_STOP
#define MIDI_ACTIVESENSE            0xfe    // MIDI_ACTIVESENSE
#define MIDI_RESET                  0xff    // MIDI_RESET

#define MIDI_STATUSBYTEFILTER       0x80    // filter for MIDI status byte to remove channel info
// a status byte must have the high bit set (0x80)
#define MIDI_SYSTEM_REALTIME_FILTER 0xf8    // filter for MIDI system realtime messages
                                            // a system realtime message must have all these bits set


// For reference, this site is better than the formal MIDI 1.0 specs in many ways
// http://midi.teragonaudio.com/tech/midispec.htm

#define MIDI_MESSAGE_IS_THREE_BYTES(status) ( \
                    status == MIDI_NOTEOFF || \
                    status == MIDI_NOTEON || \
                    status == MIDI_POLYAFTERTOUCH || \
                    status == MIDI_CONTROLCHANGE || \
                    status == MIDI_PITCHBEND || \
                    status == MIDI_SONGPOSITIONPOINTER)

#define MIDI_MESSAGE_IS_TWO_BYTES(status) ( \
                    status == MIDI_PROGRAMCHANGE || \
                    status == MIDI_MONOAFTERTOUCH || \
                    status == MIDI_TIMECODE || \
                    status == MIDI_SONGSELECT)

#define MIDI_MESSAGE_IS_ONE_BYTE(status) ( \
                    status == MIDI_TIMINGCLOCK || \
                    status == MIDI_TUNEREQUEST || \
                    status == MIDI_START || \
                    status == MIDI_CONTINUE || \
                    status == MIDI_STOP || \
                    status == MIDI_ACTIVESENSE || \
                    status == MIDI_RESET || \
                    status == MIDI_EOX)

#define MIDI_MESSAGE_TERMINATES_RUNNING_STATUS(status) (\
                    status >= MIDI_SYSEX && \
                    status <= MIDI_EOX )

#define MIDI_STATUS_SUPPORTS_RUNNING_STATUS(status) (\
                    status >= MIDI_NOTEOFF && \
                    status < MIDI_SYSEX)

#define MIDI_BYTE_IS_STATUS_BYTE(b) (\
                    (b & MIDI_STATUSBYTEFILTER) == MIDI_STATUSBYTEFILTER)

#define MIDI_BYTE_IS_SYSTEM_REALTIME_STATUS(b) (\
                    (b & MIDI_SYSTEM_REALTIME_FILTER) == MIDI_SYSTEM_REALTIME_FILTER)

#define MIDI_BYTE_IS_SYSEX_START_STATUS(b) (\
                    b == MIDI_SYSEX)

#define MIDI_BYTE_IS_SYSEX_END_STATUS(b) (\
                    b == MIDI_EOX)

#define MIDI_BYTE_IS_DATA_BYTE(b) (\
                    (b &  MIDI_STATUSBYTEFILTER) == 0)
