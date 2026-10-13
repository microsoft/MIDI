// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#ifndef PCH_H
#define PCH_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "windows.h"

// E_NOTFOUND, which midi_ksa_pin_map_property.h uses
#include <mmdeviceapi.h>

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Must precede the other wil headers: without it WIL cannot identify winrt::hresult_error
// and fail fasts instead of logging, which kills the test process instead of failing a test.
#include <wil\cppwinrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <WexTestClass.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

// the KIR which gates the behavior under test. The tests of that behavior have to no-op when it
// is disabled, otherwise a rollback turns the suite red.
#include <Feature_Servicing_MIDI2KSAPinChangesWhileOpen.h>

// generated service interfaces, so the tests talk to the service without the SDK
#include <WindowsMidiServices.h>
#include <WindowsMidiServices_i.c>
#include <Midi2MidiSrvTransport.h>

// property keys
#include "MidiDefs.h"
#include "swd_helpers.h"

namespace internal = ::WindowsMidiServicesInternal;

// the transport's pin map format, and its header-only code under test
#include "midi_ksa_pin_map_property.h"
#include "Midi2.KSAggregatePinMapUpdates.h"

#include "..\inc\MidiTestServiceHelpers.h"

#endif //PCH_H
