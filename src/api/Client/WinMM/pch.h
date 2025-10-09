// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include <windows.h>
#include <map>
#include <queue>
#include <memory>
#include <string>

#include <assert.h>
#include <wrl\module.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <mmsystem.h>
#include <mmddk.h>
#include <mmreg.h>
#include <setupapi.h>

#include <SDKDDKVer.h>

#include "resource.h"

#include "wstring_util.h"

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Devpkey.h>
#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "MidiSrvRpc.h"
#include "MidiXProc.h"
#include "MidiSrvTransport.h"

#include "midi1_message_defs.h"

class WdmAud2TelemetryProvider : public wil::TraceLoggingProvider
{
    IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
        WdmAud2TelemetryProvider,
        "Microsoft.Windows.Midi2.WdmAud2",
        // {e6443bc1-e9c5-5a3f-cfb6-abcd62e52e41}
        (0xe6443bc1,0xe9c5,0x5a3f,0xcf,0xb6,0xab,0xcd,0x62,0xe5,0x2e,0x41));

public:
};


#define HRESULT_FROM_MMRESULT(x) (x == MMSYSERR_NOERROR ? S_OK : MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, x))

MMRESULT MMRESULT_FROM_HRESULT(HRESULT hResult);

extern bool g_ProcessIsTerminating;

#ifndef WINMM_APIID
#define WINMM_APIID {0x159f7ef3,0xb022,0x4cb7,{0xad,0x3f,0x6,0x42,0x21,0xbc,0x64,0x18}}
#endif

