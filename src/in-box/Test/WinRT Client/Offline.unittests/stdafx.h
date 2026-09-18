// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning (push)
#pragma warning (disable: 4005)


#include <windows.h>

// windows.h defines GetObject, which collides with a member of the JSON projection. This has to
// come before anything that pulls the JSON headers in, and the Midi2 projections do.
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>


using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::Utilities::Messages;

#include <iostream>

#include <avrt.h>
#include <wil\cppwinrt.h>
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

#include "MidiStreamMessageBuilderTests.h"
#include "MidiCiMessageTests.h"
#include "MidiCapabilityInquiryMessageTests.h"
#include "MidiCiProgramListTests.h"
#include "MidiCiResponderTests.h"

//#include "MidiFunctionBlockMessageBuilderTests.h"
//#include "MidiMessageBuilderTests.h"
//#include "MidiStreamMessageBuilderTests.h"
//#include "MidiEndpointIdHelperTests.h"
//#include "MidiMessageConverterTests.h"



#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif


#pragma warning (pop)

#endif