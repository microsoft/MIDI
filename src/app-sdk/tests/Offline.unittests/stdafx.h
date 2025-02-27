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


#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

#include <iostream>

#include <avrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>
#include <Objbase.h>
#include "loopback_ids.h"
#include "MidiDefs.h"


#include "MidiClockTests.h"
#include "MidiGroupTests.h"
#include "MidiChannelTests.h"

#include "MidiUniqueIdTests.h"

#include "MidiMessage32Tests.h"
#include "MidiMessage64Tests.h"
#include "MidiMessage96Tests.h"
#include "MidiMessage128Tests.h"
#include "MidiMessagePacketTests.h"

#include "MidiFunctionBlockMessageBuilderTests.h"
#include "MidiMessageBuilderTests.h"
#include "MidiStreamMessageBuilderTests.h"



#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif


#pragma warning (pop)

#endif