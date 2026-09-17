// Copyright (c) Microsoft Corporation. All rights reserved.

#ifndef PCH_H
#define PCH_H

#pragma once

#include <windows.h>

// Must precede the other wil headers: without it WIL cannot identify winrt::hresult_error
// and fail fasts instead of logging, which kills the test process instead of failing a test.
#include <wil\cppwinrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <WexTestClass.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <vector>

// The library under test. These link against it directly rather than going through the service,
// so they can reach code paths no configuration payload can produce.
#include <MidiSynth/DlsCollection.h>
#include <MidiSynth/DlsTypes.h>
#include <MidiSynth/RiffReader.h>
#include <MidiSynth/SynthConfig.h>
#include <MidiSynth/SynthEngine.h>
#include <MidiSynth/UmpDispatcher.h>

#endif //PCH_H
