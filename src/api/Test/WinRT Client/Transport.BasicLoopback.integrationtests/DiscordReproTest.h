
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "setupapi.lib")

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef DRV_QUERYDEVICEINTERFACESIZE
#define DRV_QUERYDEVICEINTERFACESIZE 0x80d
#endif

#ifndef DRV_QUERYDEVICEINTERFACE
#define DRV_QUERYDEVICEINTERFACE 0x80C
#endif

#include <initguid.h>
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <setupapi.h>
#include <devpkey.h>

#include <algorithm>
#include <new>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <iostream>

#include <string>
#include <wil/com.h>
#include <wil/registry.h>
#include <wil/result.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.h>
namespace midi2 = winrt::Windows::Devices::Midi2;
#include <winrt/Windows.Devices.Midi2.Enumeration.h>
#include <winrt/Windows.Devices.Midi2.Enumeration.Legacy.h>
#include <winrt/Windows.Devices.Midi2.Transports.BasicLoopback.h>
namespace basicLoopback = winrt::Windows::Devices::Midi2::Transports::BasicLoopback;
#include <winrt/Windows.Devices.Midi2.Utilities.Messages.h>
namespace messages = winrt::Windows::Devices::Midi2::Utilities::Messages;

#include <winrt/Windows.Devices.Midi2.ClientPlugins.h>
using namespace winrt::Windows::Devices::Midi2::ClientPlugins;




#pragma once

class DiscordReproTests
    : public WEX::TestClass<DiscordReproTests>
{
public:

    BEGIN_TEST_CLASS(DiscordReproTests)
        TEST_CLASS_PROPERTY(L"TestClassification", L"Unit")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.Transports.BasicLoopback.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.Transports.Virtual.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.Reporting.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.ServiceConfig.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.Enumeration.dll")
        TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"Windows.Devices.Midi2.dll")
        END_TEST_CLASS()

        //TEST_CLASS_SETUP(ClassSetup);
        //TEST_CLASS_CLEANUP(ClassCleanup);

        //TEST_METHOD_SETUP(TestSetup);
        //TEST_METHOD_CLEANUP(TestCleanup);

        TEST_METHOD(TestMaximRepro);

private:


};


