// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <strsafe.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\win32_helpers.h>
#include <wil\registry.h>
#include <wil\result.h>
#include <wil\tracelogging.h>
#include <wil\wistd_memory.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>
#include <sddl.h>

#include "MidiAbstraction.h"

#include "Midi2SimpleLoopbackAbstraction.h"
#include "Midi2NetworkMidiAbstraction.h"
#include "Midi2VirtualMidiAbstraction.h"
#include "Midi2KSAbstraction.h"

#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "MidiSrvRpc.h"

#include "SWDevice.h"
#include <initguid.h>
#include <MMDeviceAPI.h>
#include <Devpkey.h>
#include "MidiDefs.h"

#include "MidiTelemetry.h"
#include "MidiPerformanceManager.h"
#include "MidiProcessManager.h"
#include "MidiDeviceManager.h"
#include "MidiXProc.h"

// MidiDevicePipe holds MidiClientPipe(s) that it is connected to.
// MidiClientPipe holds a MidiDevicePipe that it is connected to.
// declare these prior to including the headers.
class CMidiClientPipe;
class CMidiDevicePipe;

#include "MidiDevicePipe.h"
#include "MidiClientPipe.h"
#include "MidiClientManager.h"

#include "MidiSrv.h"



