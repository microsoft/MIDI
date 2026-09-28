// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#ifndef PCH_H
#define PCH_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
#include <bcrypt.h>
#include <objbase.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include <WexTestClass.h>

// the protocol engine has no sockets, threads or clock of its own
#include "rtpmidi_session.h"

#include "RtpMidiMdns.h"
#include "midi_dnssd_announcer.h"

#include "WindowsMidiServices.h"

// rpcndr.h defines small as char, which breaks any variable of that name
#undef small

#endif //PCH_H
