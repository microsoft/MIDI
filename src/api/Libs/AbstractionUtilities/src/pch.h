// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once


#ifndef PCH_H
#define PCH_H

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <assert.h>
#include <devioctl.h>

#include <SDKDDKVer.h>

#include <string>
#include <vector>
#include <map>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>


#include "SWDevice.h"
#include <initguid.h>
#include "setupapi.h"

#include "strsafe.h"
#include "wstring_util.h"

#include "MidiDefs.h"
//#include "MidiXProc.h"

#include "MidiDataFormat.h"

//#include "MidiDeviceManagerInterface.h"

namespace internal = ::Windows::Devices::Midi2::Internal;

#include "swd_helpers.h"
#include "json_helpers.h"


#endif //PCH_H
