// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <windows.h>
#include <winternl.h>
//#include <ntstatus.h>


#include <iostream>
#include <chrono>
#include <format>


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>

#include <winrt/Microsoft.Devices.Midi2.h>
#include <winrt/Microsoft.Devices.Midi2.Diagnostics.h>



namespace midi2 = winrt::Microsoft::Devices::Midi2;
namespace diag = winrt::Microsoft::Devices::Midi2::Diagnostics;

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

#include "combaseapi.h"
#include "wstring_util.h"

namespace internal = ::WindowsMidiServicesInternal;

#include "mididiag_field_defs.h"