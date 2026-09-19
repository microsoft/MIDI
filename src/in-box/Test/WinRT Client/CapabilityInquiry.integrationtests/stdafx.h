// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning (push)
#pragma warning (disable: 4005)

#include <windows.h>

// windows.h defines GetObject, which collides with a member of the JSON projection. This has to
// come before anything that pulls the JSON headers in, and the Midi2 projections do. It stays
// undefined afterwards, as it is in the SDK itself, so that calls to GetObject compile here too.
#undef GetObject
#include <winrt/Windows.Data.Json.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Transports.Virtual.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>

#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <avrt.h>
#include <wil\cppwinrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>
#include <Objbase.h>
#include "loopback_ids.h"
#include "MidiDefs.h"

#include "..\SdkTestCommon.h"

#include "MidiCapabilityInquiryTestResponder.h"
#include "MidiCapabilityInquirySessionTests.h"




#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#pragma warning (pop)

#endif
