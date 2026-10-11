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

#include <algorithm>
#include <map>
#include <optional>
#include <string>
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
#include <winrt/Windows.Data.Json.h>

// the KIR which gates the behavior under test. The tests have to no-op when it is disabled,
// otherwise a rollback turns the suite red.
#include <Feature_Servicing_MIDI2EndpointUniqueIdValidation.h>
#include <Feature_Servicing_MIDI2EndpointNameUtf8ByteLimit.h>
#include <Feature_Servicing_MIDI2VirtualDeviceClientReconnect.h>
#include <Feature_Servicing_MIDI2VirtualDeviceClientEndpointInUse.h>
#include <Feature_Servicing_MIDI2StreamTextUtf8.h>

// not a skip condition for the hang tests, which fail whichever way it is set
#include <Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock.h>

// generated service interfaces, so tests can push configuration the same way the SDK does
#include <WindowsMidiServices.h>
#include <WindowsMidiServices_i.c>
#include <Midi2MidiSrvTransport.h>

// property keys and stream message constants
#include "MidiDefs.h"
#include "midi_ump_message_defs.h"

#include "TransportConfigTestHelper.h"
#include "..\inc\MidiTestDeviceNodes.h"
#include "..\inc\MidiTestServiceHelpers.h"

#endif //PCH_H

