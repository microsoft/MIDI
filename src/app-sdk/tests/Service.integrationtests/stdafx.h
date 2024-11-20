// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

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
using namespace winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry;
using namespace winrt::Microsoft::Windows::Devices::Midi2::ClientPlugins;
using namespace winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;
using namespace winrt::Microsoft::Windows::Devices::Midi2::Reporting;
using namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Loopback;
using namespace winrt::Microsoft::Windows::Devices::Midi2::Endpoints::Virtual;
using namespace winrt::Microsoft::Windows::Devices::Midi2::Messages;
using namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig;

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
#include "Microsoft.Windows.Devices.Midi2.Initialization.hpp"
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

#include "MidiEndpointConnectionBufferTests.h"
#include "MidiEndpointConnectionTests.h"
#include "MidiEndpointCreationThreadTests.h"
#include "MidiEndpointDeviceWatcherTests.h"
#include "MidiEndpointListenerTests.h"
#include "MidiLoopbackEndpointTests.h"
#include "MidiMessageSchedulerTests.h"
#include "MidiInitializationTests.h"
#include "MidiSessionTests.h"
#include "MidiVirtualDeviceTests.h"



#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#endif