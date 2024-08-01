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

#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Devices.Midi.h>

namespace midi1 = ::winrt::Windows::Devices::Midi;
namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;
namespace streams = ::winrt::Windows::Storage::Streams;


#include <stdint.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>
#include <cwctype>
#include <queue>
#include <mutex>
#include <format>
#include <filesystem>


// pre-declare namespaces

namespace WindowsMidiServicesInternal {};
namespace winrt::Microsoft::Windows::Devices::Midi2 {};

namespace internal = ::WindowsMidiServicesInternal;
namespace midi2 = ::winrt::Microsoft::Windows::Devices::Midi2;


// shared
#include <midi_timestamp.h>
#include <hstring_util.h>
#include <wstring_util.h>

// SDK shared
#include <SdkTraceLogging.h>

// Project-local ------------------------------------------------

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx {};
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation {};

namespace sysex = ::winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx;
namespace implementation = ::winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation;



#include "MidiSystemExclusiveSender.h"

