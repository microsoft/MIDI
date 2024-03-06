// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Data.Json.h>

using namespace winrt;

#include <wrl\module.h>
#include <wrl\event.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

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

#include "Midi2MidiSrvAbstraction_i.c"
#include "Midi2MidiSrvAbstraction.h"

#include "MidiServicePlugin.h"
#include "MidiServicePlugin_i.c"

#include "dllmain.h"

#include <Devpkey.h>
#include "MidiDefs.h"
#include "MidiDataFormat.h"
#include "MidiFlow.h"
#include "MidiAbstraction.h"

#include "MidiSrvRpc.h"
#include "MidiXProc.h"
#include "Midi2.MidiSrvAbstraction.h"
#include "Midi2.MidiSrv.h"
#include "Midi2.MidiSrvIn.h"
#include "Midi2.MidiSrvOut.h"
#include "Midi2.MidiSrvBiDi.h"
#include "Midi2.MidiSrvConfigurationManager.h"
#include "Midi2.MidiSrvSessionTracker.h"

