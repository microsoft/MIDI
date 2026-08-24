// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <windows.h>

#include <iostream>
#include <chrono>
#include <format>
#include <conio.h>
#include <stdio.h>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Connectivity.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;
namespace net = winrt::Windows::Networking;
namespace netconn = winrt::Windows::Networking::Connectivity;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Transports.Network.h>

namespace midi2 = winrt::Windows::Devices::Midi2;
namespace midinet = winrt::Windows::Devices::Midi2::Transports::Network;


#include <wrl\module.h>
#include <wrl\event.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <atlconv.h>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include <initguid.h>

#include "wstring_util.h"
#include "resource_util.h"

namespace internal = ::WindowsMidiServicesInternal;

#include "resource.h"

