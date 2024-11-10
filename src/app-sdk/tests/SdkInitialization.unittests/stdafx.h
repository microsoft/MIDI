// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.ClientPlugins.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Reporting.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Endpoints.Loopback.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Endpoints.Virtual.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Messages.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.ServiceConfig.h>

using namespace winrt::Microsoft::Windows::Devices::Midi2;

#include <iostream>

#include <avrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>
#include <Objbase.h>
#include "loopback_ids.h"
#include "MidiDefs.h"

#include "WindowsMidiServicesClientInitialization.h"
#include "WindowsMidiServicesClientInitialization_i.c"
#include "MidiDesktopAppSdkBootstrapper.hpp"

#include "MidiAppSdkInitializationTests.h"



#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#endif