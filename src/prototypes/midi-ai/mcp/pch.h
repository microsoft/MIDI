// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. Not built by any solution and never shipped. See ..\README.md.
//
// A plain header, not a precompiled one. The shared tool sources in midi-app-shared say
// #include "pch.h" and expect their consumer to supply what they need; this is that.

#pragma once

#include <windows.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <knownfolders.h>

// windows.h turns these into ...W. Undone before any projection header is read, or
// IJsonValue::GetObject() is declared as GetObjectW() and nothing can call it.
#undef GetObject
#undef GetMessage
#undef SendMessage

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// First among the WIL headers, or a winrt::hresult_error reaching a WIL catch site fail-fasts.
#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.Loopback.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;
namespace json = ::winrt::Windows::Data::Json;
namespace midi2 = ::winrt::Windows::Devices::Midi2;
namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;
namespace midi2msg = ::winrt::Windows::Devices::Midi2::Utilities::Messages;

#include "MidiEndpointHelpers.h"
