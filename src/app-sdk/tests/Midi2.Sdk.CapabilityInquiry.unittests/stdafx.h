// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#include <windows.h>

#include <winrt/Microsoft.Windows.Devices.Midi2.CapabilityInquiry.h>

using namespace winrt::Microsoft::Windows::Devices::Midi2::CapabilityInquiry;

#include <iostream>

#include <avrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>
#include <Objbase.h>
#include "loopback_ids.h"
#include "MidiDefs.h"


#include "MidiUniqueIdTests.h"

#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#endif