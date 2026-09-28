// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Precompiled header for the rtpMIDI transport. Follows the Bluetooth MIDI transport,
// plus Winsock, which has to come before windows.h.
// ============================================================================

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <windns.h>
#include <bcrypt.h>

#include <mmdeviceapi.h>        // E_NOTFOUND
#include <hstring.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

// windows.h defines GetObject to GetObjectW, and the projection below would pick that up and
// rename IJsonValue::GetObject. ATL still expects the macro, so it comes back until ATL is in.
#undef GetObject
#include <winrt/Windows.Data.Json.h>
#define GetObject GetObjectW

namespace json = ::winrt::Windows::Data::Json;
namespace foundation = ::winrt::Windows::Foundation;

#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <avrt.h>

// Must precede the other wil headers: without it WIL cannot identify winrt::hresult_error and
// fail fasts instead of logging, turning every catch site into a process crash.
#include <wil\cppwinrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\tracelogging.h>

#include <SDKDDKVer.h>

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
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
#include "setupapi.h"

#include "strsafe.h"
#include "wstring_util.h"
#include "hstring_util.h"
#include "ump_helpers.h"
#include "midi_timestamp.h"

// ATL has been included by now, so the macro has done its last useful job
#undef GetObject

namespace internal = ::WindowsMidiServicesInternal;

#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "WindowsMidiServices_i.c"

#include "json_defs.h"
#include "json_helpers.h"
#include "json_transport_command_helper.h"
#include "swd_helpers.h"
#include "resource_util.h"

#include "MidiEndpointMatchCriteria.h"
#include "MidiEndpointCustomProperties.h"
#include "MidiEndpointCustomPropertiesCache.h"

// also brings in midi_group_terminal_blocks.h
#include "MidiEndpointNameTable.h"

#include "Feature_Servicing_MIDI2PortNamingRework.h"
#include "Feature_Servicing_MIDI2SchedulerV2.h"

#include "midi_dnssd_browser.h"
#include "midi_dnssd_announcer.h"

#include "Midi2RtpMidiTransport_i.c"
#include "Midi2RtpMidiTransport.h"

#include "dllmain.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

// libmidi2 calls sprintf in a display helper this transport never uses
#pragma warning(push)
#pragma warning(disable: 4996)
#include <libmidi2/bytestreamToUMP.h>
#include <libmidi2/umpToBytestream.h>
#pragma warning(pop)

// the protocol engine, which the tests use directly
#include "rtpmidi_session.h"

#include "transport_defs.h"
#include "rtp_json_defs.h"
#include "rtp_transport_error_codes.h"
#include "RtpMidiDefinitions.h"
#include "ThreadpoolWork.h"
#include "rtp_notification_defs.h"
#include "RtpMidiNotificationSignal.h"
#include "RtpMidiApprovals.h"
#include "RtpMidiNet.h"
#include "RtpMidiMdns.h"
#include "RtpMidiEndpointProperties.h"

class CMidi2RtpMidiEndpointManager;
class CMidi2RtpMidiConfigurationManager;
class RtpMidiNode;

#include "Midi2.RtpMidiTransport.h"
#include "RtpMidiConnection.h"
#include "RtpMidiNode.h"
#include "Midi2.RtpMidiEndpointManager.h"
#include "Midi2.RtpMidiConfigurationManager.h"
#include "TransportState.h"
#include "Midi2.RtpMidiBidi.h"
#include "Midi2.RtpMidiPluginMetadataProvider.h"
