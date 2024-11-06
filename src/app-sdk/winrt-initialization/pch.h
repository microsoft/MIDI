// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



#pragma once

//#define _VSDESIGNER_DONT_LOAD_AS_DLL

#include <unknwn.h>

#include <Windows.h>

#include <optional>
#include <string>
#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cwctype>
#include <queue>
#include <mutex>
#include <format>
#include <filesystem>

#include <chrono>


#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <wil\registry.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Devices.Midi.h>

#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;
namespace enumeration = ::winrt::Windows::Devices::Enumeration;
namespace midi1 = ::winrt::Windows::Devices::Midi;
namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;



// pre-declare namespaces

namespace WindowsMidiServicesInternal {};
namespace winrt::Microsoft::Windows::Devices::Midi2 {};

namespace internal = ::WindowsMidiServicesInternal;
namespace midi2 = ::winrt::Microsoft::Windows::Devices::Midi2;

#include <WindowsMidiServicesVersion.h>

#include <Devpropdef.h>
#include <MidiDefs.h>

// shared
#include <json_defs.h>
#include <hstring_util.h>
#include <wstring_util.h>
#include <json_helpers.h>
#include <resource_util.h>


// service interface
#include <WindowsMidiServices.h>
#include <WindowsMidiServices_i.c>
#include <Midi2MidiSrvAbstraction.h>

// SDK shared
#include <SdkTraceLogging.h>

// Project-local ------------------------------------------------

namespace winrt::Microsoft::Windows::Devices::Midi2::Initializer {};
namespace winrt::Microsoft::Windows::Devices::Midi2::Initializer::implementation {};
namespace init = ::winrt::Microsoft::Windows::Devices::Midi2::Initializer;
namespace implementation = ::winrt::Microsoft::Windows::Devices::Midi2::Initializer::implementation;

#include "resource.h"

#include "MidiServicesInitializer.h"
