// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <unknwn.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;
namespace streams = ::winrt::Windows::Storage::Streams;


// needed in code for MIDI_ROOT_NAMESPACE_CPP
#include "winrt_midi1_midl_defines.h"

// forward declare these namespaces so we can alias them
namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation {}
namespace winrt::MIDI_ROOT_NAMESPACE_CPP {}

namespace implementation = ::winrt::MIDI_ROOT_NAMESPACE_CPP::implementation;
namespace midi1 = ::winrt::MIDI_ROOT_NAMESPACE_CPP;

