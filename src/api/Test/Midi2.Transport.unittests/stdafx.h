// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once


// this is a hack, but because this is a test project, and know we're not
// using coroutines in our tests, we'll risk it. 
#define _ALLOW_COROUTINE_ABI_MISMATCH


#include <windows.h>
#include <cguid.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <ks.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>

#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include <Devpkey.h>
#include "MidiSwEnum.h"
#include <initguid.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiXProc.h"
#include <mmdeviceapi.h>
#include "KsHandleWrapper.h"
#include "MidiKsEnum.h"

#include <wil/resource.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <mmsystem.h>
#include <mmeapi.h>
#include <mmddk.h>

#include "MidiTestCommon.h"

#include "Midi2MidiSrvTransport.h"
#include "Midi2KSTransport.h"
#include "Midi2KSAggregateTransport.h"

#include "MinMidiControl.h"

