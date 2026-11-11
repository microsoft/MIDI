// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning (push)
#pragma warning (disable: 4005)

// the test remote needs Winsock ahead of windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
#include <bcrypt.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;
namespace json = winrt::Windows::Data::Json;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.ServiceConfig.h>
#include <winrt/Windows.Devices.Midi2.Transports.Rtp.h>

using namespace winrt::Windows::Devices::Midi2::Transports::Rtp;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <wil\cppwinrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <WexTestClass.h>

#include "..\..\inc\MidiTestDeviceNodes.h"

// rpcndr.h defines small as char, which breaks any variable of that name
#undef small

// The protocol engine is header only. With the transport tests' remote, it lets these tests
// be a real RTP-MIDI device on loopback without a second machine.
#include "rtpmidi_session.h"
#include "RtpMidiTestPeer.h"

#pragma warning (pop)

#endif
