// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <wrl\module.h>
#include <wrl\event.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <ppltasks.h>

#include <SDKDDKVer.h>

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // some CString constructors will be explicit
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include "mididefs.h"

#include <libmidi2/utils.h>                 // AM_MIDI2
#include <libmidi2/umpMessageCreate.h>      // AM_MIDI2
#include <libmidi2/umpToMIDI1Protocol.h>    // AM_MIDI2
//#include <libmidi2/>

#include "ump_helpers.h"        // internal helpers
#include "wstring_util.h"
#include "swd_helpers.h"

namespace internal = ::WindowsMidiServicesInternal;

#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "Midi2UmpProtocolDownscalerTransform_i.c"
#include "Midi2UmpProtocolDownscalerTransform.h"

#include "dllmain.h"

#include "Midi2.UmpProtocolDownscalerTransform.h"
#include "Midi2.UmpProtocolDownscalerMidiTransform.h"

