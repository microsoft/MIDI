// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning (push)
#pragma warning (disable: 4005)

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.Bluetooth.h>

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::ServiceConfig;
using namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth;

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <wil\cppwinrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <WexTestClass.h>

#pragma warning (pop)

#endif
