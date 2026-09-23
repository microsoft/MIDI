// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// DEVELOPER DIAGNOSTIC. Not shipped, not installed, not a supported tool.
//
// Dumps what a MIDI-CI device answers with, exactly as it sent it, so a real device's payloads
// can be kept as a reference for the test rig instead of ones we made up.
// ============================================================================

#pragma once

#include <windows.h>

#include <winrt/base.h>

// windows.h defines GetObject, which breaks the JSON projection. Has to be undone before the
// Midi2 headers pull the implementation in.
#pragma push_macro("GetObject")
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#pragma pop_macro("GetObject")

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.CapabilityInquiry.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
