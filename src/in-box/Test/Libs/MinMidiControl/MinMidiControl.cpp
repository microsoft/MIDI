// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>

#include <cguid.h>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <format>
#include <conio.h>
#include <stdio.h>

#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>

#include <wrl\module.h>
#include <wrl\event.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <wil\result_macros.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Data.Json.h>

#include <filesystem>

#include <Devpkey.h>

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "WindowsMidiServices.h"
#include "MidiKSCommon.h"
#include "MinMidiControl.h"

using namespace winrt;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

HRESULT
SendDriverCommand(KSPROPERTY_MINMIDICONTROL operation, DWORD data)
{
    RETURN_HR_IF(E_INVALIDARG, operation != KSPROPERTY_MINMIDICONTROL_ADDPORT && operation != KSPROPERTY_MINMIDICONTROL_REMOVEPORT && operation != KSPROPERTY_MINMIDICONTROL_SURPRISEREMOVESIMULATION);

    // KSPROPSETID_MinMidiControl is also the interface class for the minmidicontrol interface.
    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{FE0D0E1F-6C68-4397-8DC6-A6A8DFEE70A5}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    auto deviceList = DeviceInformation::FindAllAsync(deviceSelector).get();

    for (auto const& device : deviceList)
    {
        wil::unique_hfile deviceHandle(CreateFileW(  
                                        device.Id().c_str(),
                                        GENERIC_READ | GENERIC_WRITE,
                                        0,
                                        NULL,
                                        OPEN_EXISTING,
                                        FILE_FLAG_SESSION_AWARE | FILE_ATTRIBUTE_NORMAL,
                                        NULL));

        if (!deviceHandle)
        {
            DWORD dwError = GetLastError();
            dwError = (dwError != ERROR_SUCCESS ? dwError : ERROR_GEN_FAILURE);
            RETURN_IF_FAILED(HRESULT_FROM_WIN32(dwError));
        }

        KSPROPERTY prop {0};

        prop.Id = operation;
        prop.Set = KSPROPSETID_MinMidiControl;
        prop.Flags = KSPROPERTY_TYPE_SET;
        
        RETURN_IF_FAILED(SyncIoctl(
            deviceHandle.get(),
            IOCTL_KS_PROPERTY,
            &prop,
            sizeof(KSPROPERTY),
            &data,
            sizeof(data),
            nullptr
        ));
    }

    return S_OK;
}

