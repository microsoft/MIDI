#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS

#include <hstring.h>

#include <Windows.Devices.Enumeration.h>
#include <assert.h>
//#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
//#include <ks.h>
//#include <ksmedia.h>
//#include <avrt.h>
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

#include "MidiAbstraction.h"


#include "Midi2LoopbackTransportAbstraction_i.c"
#include "Midi2LoopbackTransportAbstraction.h"

#include "dllmain.h"


#include "Midi2LoopbackTransportAbstraction.h"
//#include "Midi2LoopbackTransportAbstraction_h.h"
#include "Midi2LoopbackEndpoint.h"

