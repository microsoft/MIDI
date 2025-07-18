// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

// this is a hack, but because this is a test project, and know we're not
// using coroutines in our tests, we'll risk it. 
#define _ALLOW_COROUTINE_ABI_MISMATCH

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\win32_helpers.h>
#include <wil\registry.h>
#include <wil\result.h>
#include <wil\wistd_memory.h>
#include <ppltasks.h>
#include <WexTestClass.h>

#include "MidiSrvRpc.h"

#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

