// Copyright (c) Microsoft Corporation. All rights reserved.

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <windows.h>
#include <mmdeviceapi.h>        // mostly for E_NOTFOUND
#include <hstring.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.Streams.h>

#include <winrt/windows.devices.bluetooth.h>
#include <winrt/windows.devices.bluetooth.advertisement.h>
#include <winrt/windows.devices.bluetooth.genericattributeprofile.h>

namespace json = ::winrt::Windows::Data::Json;
namespace enumeration = ::winrt::Windows::Devices::Enumeration;
namespace foundation = ::winrt::Windows::Foundation;
namespace bt = ::winrt::Windows::Devices::Bluetooth;
namespace gatt = ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
//#include <ks.h>
//#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>
#include <ppltasks.h>

#include <SDKDDKVer.h>

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // some CString constructors will be explicit
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW

#include <codecvt>


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
#include "setupapi.h"
//#include "Devpkey.h"

#include "strsafe.h"
#include "wstring_util.h"
#include "hstring_util.h"
#include "ump_helpers.h"
#include "midi_timestamp.h"

#undef GetObject
#include <winrt/Windows.Data.Json.h>
namespace json = ::winrt::Windows::Data::Json;


// AbstractionUtilities
#include "wstring_util.h"
namespace internal = ::WindowsMidiServicesInternal;

#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"
#include "resource_util.h"

#include "MidiXProc.h"

#include "Midi2Ble1MidiTransport_i.c"
#include "Midi2Ble1MidiTransport.h"

#include "dllmain.h"


// subset of boost is installed via vcpkg https://vcpkg.io/en/package/boost-circular-buffer.html
//#include "boost/circular_buffer.hpp"

#include "transport_defs.h"
#include "ble1_json_defs.h"

#include "midi_ble_utilities.h"

class CMidi2Ble1MidiEndpointManager;
class CMidi2Ble1MidiConfigurationManager;
class MidiBleAdvertiser;

#include "TransportState.h"

#include "Midi2.Ble1MidiTransport.h"
#include "Midi2.Ble1MidiBidi.h"
#include "Midi2.Ble1MidiEndpointManager.h"
#include "Midi2.Ble1MidiConfigurationManager.h"
#include "Midi2.Ble1MidiPluginMetadataProvider.h"

#include "MidiBleAdvertiser.h"

