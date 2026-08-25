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
#include <string>
#include <vector>

#include <WexTestClass.h>

// The codec is header only and has no dependency on Bluetooth, the service or COM, which is
// what makes these pure unit tests.
#include "midi_ble_midi1_codec.h"

#endif //PCH_H
