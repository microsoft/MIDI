// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>
#include <shellapi.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Must come before the other WIL headers: it installs the translator that teaches WIL about
// cppwinrt exceptions.
#include <wil/cppwinrt.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#undef GetCurrentTime
#undef small

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>

#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include <winrt/Windows.Devices.Midi2.h>

namespace winrt
{
    namespace foundation = ::winrt::Windows::Foundation;
    namespace numerics = ::winrt::Windows::Foundation::Numerics;
    namespace xaml = ::winrt::Microsoft::UI::Xaml;
    namespace comp = ::winrt::Microsoft::UI::Composition;
    namespace midi2 = ::winrt::Windows::Devices::Midi2;
}

// Custom types used from XAML markup but with no .xaml file of their own are not emitted into
// XamlTypeInfo.g.cpp unless their headers are visible here.
#include "GlassControl.h"
#include "HybridControl.h"
#include "SurfaceAutomationPeer.h"
