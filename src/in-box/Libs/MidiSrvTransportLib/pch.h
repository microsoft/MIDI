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

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include <filesystem>

#include <Devpkey.h>
#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "MidiSrvRpc.h"
#include "MidiXProc.h"

