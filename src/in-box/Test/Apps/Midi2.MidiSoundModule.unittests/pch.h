// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

// Test bodies are written inside the class.
#define INLINE_TEST_METHOD_MARKUP
#include <WexTestClass.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "SoundFont.h"
#include "Synthesizer.h"
#include "SynthCore.h"
#include "SynthPropertyRequests.h"
#include "SysEx7.h"

#include "MidiCiMessage.h"

#include "Sf2Builder.h"
