// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <hstring.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
//#include <ks.h>
//#include <ksmedia.h>
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

#include "SWDevice.h"
#include <initguid.h>
#include "setupapi.h"
#include "Devpkey.h"

#include "strsafe.h"


#include "Midi2SimpleLoopbackAbstraction_i.c"
#include "Midi2SimpleLoopbackAbstraction.h"

#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "dllmain.h"

#include "MidiDefs.h"
#include "MidiXProc.h"


#include "Midi2.SimpleLoopbackAbstraction.h"
#include "Midi2.SimpleLoopbackMidiIn.h"
#include "Midi2.SimpleLoopbackMidiOut.h"
#include "Midi2.SimpleLoopbackMidiBiDi.h"
#include "Midi2.SimpleLoopbackEndpointManager.h"

#include "MidiLoopbackDevice.h"
#include "MidiLoopbackBidiDevice.h"
#include "MidiDeviceTable.h"
