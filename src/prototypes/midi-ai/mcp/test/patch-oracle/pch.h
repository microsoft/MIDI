// PROTOTYPE test helper. A stand-in for MIDI Patchbay's precompiled header, so the app's REAL
// MessageFilter.cpp and MessageTransform.cpp compile into a console program without WinUI.

#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <sal.h>

#undef GetObject

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>

namespace foundation = ::winrt::Windows::Foundation;
namespace json = ::winrt::Windows::Data::Json;
namespace midi2msg = ::winrt::Windows::Devices::Midi2::Utilities::Messages;
