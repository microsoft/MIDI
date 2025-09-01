// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

#include <windows.h>
#include <winternl.h>

//#include <ntstatus.h>

#pragma warning (push)
#pragma warning (disable: 4005)
#include <ntstatus.h>
#pragma warning (pop)


#include <iostream>
#include <chrono>
#include <format>

#undef GetObject


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;


#include <winrt/Microsoft.Windows.Devices.Midi2.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.Reporting.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.ServiceConfig.h>


namespace midi2 = winrt::Microsoft::Windows::Devices::Midi2;
namespace diag = winrt::Microsoft::Windows::Devices::Midi2::Diagnostics;
namespace rept = winrt::Microsoft::Windows::Devices::Midi2::Reporting;
namespace svc = winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig;




#include "combaseapi.h"
#include <mmsystem.h>
#include <timeapi.h>

#include <wil/common.h>
#include <wil/com.h>
#include <wil/registry.h>

#include <WindowsMidiServicesVersion.h>
#include "WindowsMidiServices.h"

//#include "WindowsMidiServicesClientInitialization.h"
//#include "WindowsMidiServicesClientInitialization_i.c"

#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

#include "wstring_util.h"
#include "MidiDefs.h"
#include "swd_helpers.h"

namespace internal = ::WindowsMidiServicesInternal;
//#include "midi_naming.h"
#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointNameTable.h"

#include "mididiag_field_defs.h"