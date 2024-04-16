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

#include <hstring.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Data.Json.h>

namespace json = ::winrt::Windows::Data::Json;
namespace enumeration = ::winrt::Windows::Devices::Enumeration;
namespace foundation = ::winrt::Windows::Foundation;
namespace collections = ::winrt::Windows::Foundation::Collections;

namespace bt = ::winrt::Windows::Devices::Bluetooth;
namespace gatt = ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
namespace ad = ::winrt::Windows::Devices::Bluetooth::Advertisement;

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

#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>

#include <winmeta.h>
#include <TraceLoggingProvider.h>

#include <vector>
#include <string>

#include "SWDevice.h"
#include <initguid.h>
#include "setupapi.h"
//#include "Devpkey.h"

#include "wstring_util.h"
namespace internal = ::Windows::Devices::Midi2::Internal;

#include "MidiDefs.h"
#include "MidiDataFormat.h"
#include "MidiFlow.h"
#include "MidiAbstraction.h"

#include "json_defs.h"
#include "json_helpers.h"
#include "swd_helpers.h"
#include "resource_util.h"

#include "MidiXProc.h"


class CMidi2BluetoothMidiConfigurationManager;
class CMidi2BluetoothMidiEndpointManager;
class CMidi2BluetoothMidiPluginMetadataManager;

#include "strsafe.h"

#include "abstraction_defs.h"
#include "midi_timestamp.h"
#include "ble_timestamp.h"
#include "ble_utilities.h"

#include "AbstractionState.h"

#include "MidiServicePlugin.h"
#include "MidiServicePlugin_i.c"

#include "Midi2BluetoothMidiAbstraction_i.c"
#include "Midi2BluetoothMidiAbstraction.h"

#include "mididevicemanagerinterface_i.c"
#include "mididevicemanagerinterface.h"

#include "dllmain.h"

#include "MidiBluetoothPacket.h"

#include "MidiBluetoothDeviceDefinition.h"
#include "MidiEndpointTable.h"

#include "Midi2.BluetoothMidiAbstraction.h"
#include "Midi2.BluetoothMidiBiDi.h"
#include "Midi2.BluetoothMidiEndpointManager.h"
#include "Midi2.BluetoothMidiConfigurationManager.h"
#include "Midi2.BluetoothMidiPluginMetadataProvider.h"

