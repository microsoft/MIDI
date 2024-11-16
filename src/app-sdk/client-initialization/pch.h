// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>
#include <pathcch.h>

#include <winrt/Windows.Foundation.h>
#include <hstring.h>
#include <string>

#include <synchapi.h>
#include <roapi.h>
#include <activationregistration.h>
#include <combaseapi.h>
#include <wrl.h>
#include <ctxtcall.h>
#include <Processthreadsapi.h>
#include <activation.h>
#include <VersionHelpers.h>
#include <memory>
#include <xmllite.h>


#include <wrl/module.h>
#include <wrl/event.h>
#include <avrt.h>

#include <optional>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/result_macros.h>
#include <wil/tracelogging.h>
#include <wil/registry.h>
#include <wil/filesystem.h>

#include <ppltasks.h>

#include <SDKDDKVer.h>

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

#include <Appmodel.h>
#include <comdef.h>
#include <RoMetadataApi.h>
//#include <RoMetadata.h>
#include "../undocked-reg-free-winrt/detours/detours.h"

#include "wstring_util.h"
#include "hstring_util.h"


#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"
#include "Midi2MidiSrvTransport.h"
#include "MidiDefs.h"
#include "WindowsMidiServicesVersion.h"

#include "WindowsMidiServicesClientInitialization_i.c"
#include "WindowsMidiServicesClientInitialization.h"

#include "MidiAppSdkManifest.h"
#include "MidiAppSdkRuntimeComponent.h"
#include "MidiAppSdkRuntimeComponentCatalog.h"
#include "MidiAppSdkRuntimeTypeResolution.h"
#include "MidiRoDetours.h"

#include "MidiClientInitializer.h"

#include "dllmain.h"


