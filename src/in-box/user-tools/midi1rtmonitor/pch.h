// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

// wil/cppwinrt.h MUST be the first wil include, or a winrt::hresult_error reaching any
// WIL catch macro fail-fasts the process instead of being logged.
#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>
#include <winrt/Windows.Storage.Streams.h>

#include "midi1_message_defs.h"
#include "midi_timestamp.h"
#include "resource_util.h"

#include "resource.h"

namespace internal = ::WindowsMidiServicesInternal;
