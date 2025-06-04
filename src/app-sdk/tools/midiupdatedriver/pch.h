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
//#include "swd_helpers.h"


namespace internal = ::WindowsMidiServicesInternal;

#include "initguid.h"
#include "usbiodef.h"

#include "ks.h"
#include "ksmedia.h"

// for device installation routines
#include "SetupAPI.h"
#include "newdev.h"