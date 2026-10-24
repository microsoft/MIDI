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
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Data.Json.h>

#include <WexTestClass.h>

// The codec is header only and has no dependency on Bluetooth, the service or COM, which is
// what makes these pure unit tests.
#include "midi_ble_midi1_codec.h"

// Same again for the validation helpers: everything here is decided from a string, a number or
// a json value, so none of it needs a radio or a running service.
#include "midi_ble_validation.h"
#include "transport_defs.h"
#include "bluetooth_transport_error_codes.h"

// The awaiter every Bluetooth call goes through, and the worker wakeup both background threads
// use. Plain Win32, WinRT Foundation and wil, so they are tested on their own. The awaiter comes
// first because it brings in wil/cppwinrt.h, which has to precede the other wil headers.
#include "midi_ble_await.h"
#include "midi_ble_worker_wakeup.h"

#endif //PCH_H
