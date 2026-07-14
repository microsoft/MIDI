// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning (push)
#pragma warning (disable: 4005)

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.ClientPlugins.h>
#include <winrt/Windows.Devices.Midi2.Reporting.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>


using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Diagnostics;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::ClientPlugins;
using namespace winrt::Windows::Devices::Midi2::Reporting;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;

#include <iostream>

#include <avrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>
#include <Objbase.h>
#include "loopback_ids.h"
#include "MidiDefs.h"

#include "..\SdkTestCommon.h"

#include "MidiEndpointConnectionBufferTests.h"
#include "MidiEndpointConnectionTests.h"
#include "MidiEndpointCreationThreadTests.h"
#include "MidiEndpointListenerTests.h"
#include "MidiMessageSchedulerTests.h"
#include "MidiSessionTests.h"



#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#pragma warning (pop)

#endif