// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <shellapi.h>

#include <atomic>
#include <array>
#include <chrono>
#include <conio.h>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// wil/cppwinrt.h MUST be the first wil include, or a winrt::hresult_error reaching any
// WIL catch macro fail-fasts the process instead of being logged.
#include <wil/cppwinrt.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.ClientPlugins.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.Reporting.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>
#include <winrt/Windows.Devices.Midi2.Transports.Bluetooth.h>
#include <winrt/Windows.Devices.Midi2.Transports.Loopback.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>
#include <winrt/Windows.Devices.Midi2.Utilities.SysExTransfer.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;
namespace midi2 = winrt::Windows::Devices::Midi2;
namespace midi2enum = winrt::Windows::Devices::Midi2::Enumeration;
namespace midi2legacy = winrt::Windows::Devices::Midi2::Enumeration::Legacy;
namespace midi2msg = winrt::Windows::Devices::Midi2::Utilities::Messages;
namespace midi2sysex = winrt::Windows::Devices::Midi2::Utilities::SysExTransfer;
namespace midi2report = winrt::Windows::Devices::Midi2::Reporting;
namespace midi2config = winrt::Windows::Devices::Midi2::ServiceConfig;
namespace midi2diag = winrt::Windows::Devices::Midi2::Diagnostics;
namespace midi2loop = winrt::Windows::Devices::Midi2::Transports::Loopback;
namespace midi2basicloop = winrt::Windows::Devices::Midi2::Transports::BasicLoopback;
namespace midi2bt = winrt::Windows::Devices::Midi2::Transports::Bluetooth;

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>

#include "resource.h"
