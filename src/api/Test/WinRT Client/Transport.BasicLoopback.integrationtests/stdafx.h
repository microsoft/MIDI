// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning (push)
#pragma warning (disable: 4005)

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

#include <winrt/Windows.Devices.Midi2.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>

// The older WinRT MIDI 1.0 API. Deliberately aliased rather than pulled in with a
// using-directive, because several of its type names collide with Midi2 ones.
#include <winrt/Windows.Devices.Midi.h>
namespace midi1 = winrt::Windows::Devices::Midi;

using namespace winrt::Windows::Devices::Midi2;
using namespace winrt::Windows::Devices::Midi2::Enumeration;
using namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy;
using namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback;

#include <iostream>

#include <avrt.h>
#include <mmsystem.h>
#include <wil\cppwinrt.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>
#include <WexTestClass.h>
#include <Objbase.h>
#include "loopback_ids.h"
#include "MidiDefs.h"

#include "..\SdkTestCommon.h"

// these are artifacts of the com-extensions-idl project
#include "WindowsMidiServicesAppSdkComExtensions.h"


// Resolves the current WinMM port number for an endpoint and flow.
//
// The service assigns the port number asynchronously, and it can arrive after the
// port is first enumerated, so we re-query the port and retry until the number is
// within the range WinMM will accept.
inline bool TryResolveWinMMPortNumber(
    _In_ winrt::hstring const& endpointDeviceId,
    _In_ Midi1PortFlow const flow,
    _Out_ uint32_t& portNumber)
{
    portNumber = 0;

    const int maxAttempts = 50;     // up to 5 seconds

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        auto ports = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointDeviceId, flow);

        if (ports != nullptr && ports.Size() > 0)
        {
            auto number = ports.GetAt(0).Number();

            auto deviceCount = (flow == Midi1PortFlow::MidiMessageSource)
                ? midiInGetNumDevs()
                : midiOutGetNumDevs();

            if (number < deviceCount)
            {
                portNumber = number;
                return true;
            }
        }

        Sleep(100);
    }

    return false;
}


// Resolves the WinRT MIDI 1.0 port device id for an endpoint and flow. The port
// device id doubles as the device id accepted by MidiInPort/MidiOutPort::FromIdAsync.
// The ports are created a little after the endpoint, so this retries.
inline bool TryResolveMidi1PortDeviceId(
    _In_ winrt::hstring const& endpointDeviceId,
    _In_ Midi1PortFlow const flow,
    _Out_ winrt::hstring& portDeviceId)
{
    portDeviceId = L"";

    const int maxAttempts = 50;     // up to 5 seconds

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        auto ports = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointDeviceId, flow);

        if (ports != nullptr && ports.Size() > 0)
        {
            auto id = ports.GetAt(0).PortDeviceId();

            if (!id.empty())
            {
                portDeviceId = id;
                return true;
            }
        }

        Sleep(100);
    }

    return false;
}

#include <functional>

#include "MidiBasicLoopbackTests.h"
#include "MidiBasicLoopbackBenchmarks.h"


#ifndef LOG_OUTPUT
#define LOG_OUTPUT(fmt, ...)  WEX::Logging::Log::Comment(WEX::Common::String().Format(fmt, __VA_ARGS__))
#endif

#pragma warning (pop)

#endif