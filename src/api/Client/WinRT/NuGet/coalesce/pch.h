// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

//#include <windows.h>
//#include <winternl.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;
namespace enumeration = winrt::Windows::Devices::Enumeration;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.ClientPlugins.h>
#include <winrt/Windows.Devices.Midi2.Diagnostics.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.Reporting.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>
#include <winrt/Windows.Devices.Midi2.Transports.Loopback.h>
#include <winrt/Windows.Devices.Midi2.Transports.Virtual.h>
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>


//#include "combaseapi.h"
