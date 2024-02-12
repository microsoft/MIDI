// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Data.Json.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <SDKDDKVer.h>

namespace json = winrt::Windows::Data::Json;


#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // some CString constructors will be explicit
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include "SWDevice.h"
#include <initguid.h>
#include "Devpkey.h"
#include <mmdeviceapi.h>

#include "strsafe.h"

#include "abstraction_defs.h"
#include "midi_config_json.h"

#include "midiabstraction_i.c"
#include "midiabstraction.h"

#include "Midi2KSAbstraction_i.c"
#include "Midi2KSAbstraction.h"

#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "midiendpointprotocolmanagerinterface_i.c"
#include "midiendpointprotocolmanagerinterface.h"

#include "dllmain.h"

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include "MidiKs.h"


#include "Midi2.KSAbstraction.h"
#include "Midi2.KSMidi.h"
#include "Midi2.KSMidiIn.h"
#include "Midi2.KSMidiOut.h"
#include "Midi2.KSMidiBiDi.h"
#include "Midi2.KSMidiEndpointManager.h"
#include "Midi2.KSMidiConfigurationManager.h"


