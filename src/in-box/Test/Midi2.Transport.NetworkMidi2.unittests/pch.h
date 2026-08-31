// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here

// winsock2 must come before windows.h, otherwise windows.h pulls in the original winsock.h and
// every socket type is defined twice
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "windows.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

// Must precede the other wil headers: without it WIL cannot identify winrt::hresult_error
// and fail fasts instead of logging, which kills the test process instead of failing a test.
#include <wil\cppwinrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Networking.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Networking.ServiceDiscovery.Dnssd.h>

#include "MidiSequenceNumber.h" // this is in the network midi project
#include "network_transport_error_codes.h" // shared with the service and the SDK

// generated service interfaces, so tests can push configuration the same way the SDK does
#include <WindowsMidiServices.h>
#include <WindowsMidiServices_i.c>
#include <Midi2MidiSrvTransport.h>

#include "SequenceNumberTests.h"
#include "AuthenticationDigestTests.h"

#include "NetworkMidiTestProtocol.h"
#include "NetworkMidiTestMdns.h"
#include "NetworkMidiTestClient.h"
#include "NetworkMidiTestFakeHost.h"
#include "NetworkMidiTestServiceConfig.h"
#include "NetworkMidiTestHostLocator.h"
#include "NetworkMidiTestContext.h"
#include "NetworkMidiSessionTests.h"
#include "NetworkMidiApprovalTests.h"
#include "NetworkMidiTransportSettingsTests.h"
#include "NetworkMidiErrorTests.h"
#include "NetworkMidiMalformedTests.h"
#include "NetworkMidiClientTests.h"

#endif //PCH_H
