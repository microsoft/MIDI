// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#pragma warning (push)
#pragma warning (disable: 4005)

#include <windows.h>
//#include <winternl.h>

//#include <ntstatus.h>

#include <iostream>
#include <chrono>
#include <format>
#include <conio.h>
#include <stdio.h>


#include <wrl\module.h>
#include <wrl\event.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\registry.h>
#include <wil\registry_helpers.h>

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
#include "midi1_message_defs.h"

namespace internal = ::WindowsMidiServicesInternal;

#define KEY_ESCAPE 0x1B
#define KEY_SPACE  0x20


#pragma warning (pop)
