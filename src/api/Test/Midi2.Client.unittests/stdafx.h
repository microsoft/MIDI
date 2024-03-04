// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <winrt/Windows.Devices.Midi2.h>

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
#include "MidiEndpointConnectionBufferTests.h"
#include "MidiEndpointConnectionTests.h"
#include "MidiEndpointCreationThreadTests.h"
#include "MidiEndpointDeviceWatcherTests.h"
#include "MidiEndpointListenerTests.h"
#include "MidiFunctionBlockMessageBuilderTests.h"
#include "MidiMessageBuilderTests.h"
#include "MidiMessagePacketTests.h"
#include "MidiMessageSchedulerTests.h"
#include "MidiSessionTests.h"
#include "MidiStreamMessageBuilderTests.h"

#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#endif