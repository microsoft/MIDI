// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

// this is a hack, but because this is a test project, and we know we're not
// using coroutines in our tests, we'll risk it. 
#define _ALLOW_COROUTINE_ABI_MISMATCH


#include <windows.h>
#include <cguid.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>

#include "MidiTestCommon.h"

#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

