// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

//#define _VSDESIGNER_DONT_LOAD_AS_DLL


#include <unknwn.h>

#include <Windows.h>

//#include <wil/cppwinrt.h> // must be before the first C++ WinRT header
//#include <wil/result.h>


#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>

using namespace winrt;


#include <stdint.h>

// internal
#include "midi_service_interface.h"
#include "ump_helpers.h"
#include "memory_buffer.h"


// shared
#include <midi_ump.h>

namespace internal = ::Windows::Devices::Midi2::Internal;
//namespace intshared = ::Windows::Devices::Midi2::Internal::Shared;
//namespace intump = ::Windows::Devices::Midi2::Internal::SharedUmp;
