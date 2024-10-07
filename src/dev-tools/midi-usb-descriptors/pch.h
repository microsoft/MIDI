// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================
// Portions of this code adapted from the Windows UsbView Sample application
// Licenses and restrictions for that code apply

#pragma once

#include <windows.h>
#include <winternl.h>

#include <ntstatus.h>

//#pragma warning (disable: 4005)
//#include <ntstatus.h>
//#pragma warning (pop)


#include <iostream>
#include <chrono>
#include <format>


#include "combaseapi.h"
#include <mmsystem.h>
#include <timeapi.h>

#include "wstring_util.h"

#include "midiusbdescriptors_field_defs.h"

#include "general_descriptors.h"
#include "audio_descriptors.h"
#include "midi_descriptors.h"

#include "UsbDevice.h"
