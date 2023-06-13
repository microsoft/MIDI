// Copyright (c) Microsoft Corporation. All rights reserved.
#include <ntstatus.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS

#include <Windows.Devices.Enumeration.h>
#include <assert.h>
#include <devioctl.h>
#include <wrl\implements.h>
#include <wrl\module.h>
#include <wrl\event.h>
#include <ks.h>
#include <ksmedia.h>
#include <avrt.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>
#include <ppltasks.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlcoll.h>
#include <atlsync.h>


#include "MidiKsCommon.h"
#include "MidiKsEnum.h"

using namespace concurrency;
using namespace ABI::Windows::Devices::Enumeration;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

using CAddedHandler = __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_Windows__CDevices__CEnumeration__CDeviceInformation;
using CStoppedHandler = __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_IInspectable;
using CEnumerationCompletedHandler = __FITypedEventHandler_2_Windows__CDevices__CEnumeration__CDeviceWatcher_IInspectable;

KSMidiDeviceEnum::KSMidiDeviceEnum()
{
}

KSMidiDeviceEnum::~KSMidiDeviceEnum()
{
    Cleanup();
}

HRESULT
KSMidiDeviceEnum::EnumerateFilters()
{
    HStringReference deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");
    wil::com_ptr_nothrow<IDeviceInformationStatics> devInfoStatics;
    wil::com_ptr_nothrow<IDeviceWatcher> watcher;
    EventRegistrationToken tokenAdded;
    EventRegistrationToken tokenStopped;
    EventRegistrationToken tokenEnumerationCompleted;
    wil::unique_event enumerationCompleted{wil::EventOptions::None};

    RETURN_IF_FAILED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(), &devInfoStatics));

    RETURN_IF_FAILED(devInfoStatics->CreateWatcherAqsFilter(deviceSelector.Get(), &watcher));

    auto deviceAddedHandler = Callback<CAddedHandler>([&](IDeviceWatcher* , IDeviceInformation* device) -> HRESULT
    {
        wil::unique_handle hFilter;
        HString strDevName;
        HString strDevId;
        PCWSTR deviceName = nullptr;
        PCWSTR deviceId = nullptr;
        ULONG cPins = 0;

        RETURN_IF_FAILED(device->get_Id(strDevId.GetAddressOf()));
        RETURN_IF_FAILED(device->get_Name(strDevName.GetAddressOf()));
        deviceName = WindowsGetStringRawBuffer(strDevName.Get(), NULL);
        deviceId = WindowsGetStringRawBuffer(strDevId.Get(), NULL);

        RETURN_IF_FAILED(FilterInstantiate(deviceId, &hFilter));
        RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins)));

        for (UINT i = 0; i < cPins; i++)
        {
            wil::unique_handle hPin;
            KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
            BOOL cyclic = FALSE;
            BOOL standard = FALSE;
            BOOL UMP = TRUE;
            KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;

            RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION)));

            // The external connector pin representing the phsyical connection
            // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
            // have a valid communication. Skip connector pins.
            if (KSPIN_COMMUNICATION_NONE == communication)
            {
                continue;
            }

            // attempt to instantiate using standard streaming,
            // and set that flag if we can.
            if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, FALSE, TRUE, &hPin)))
            {
                standard = TRUE;
                hPin.reset();
            }

            // attempt to instantiate using cyclic streaming,
            // and set that flag if we can.
            if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, TRUE, TRUE, &hPin)))
            {
                cyclic = TRUE;
                hPin.reset();
            }

            // attempt to instantiate using cyclic streaming,
            // and set that flag if we can.
            if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, FALSE, FALSE, &hPin)))
            {
                UMP = FALSE;
                hPin.reset();
            }

            // if this pin supports neither, then it's not a streaming pin,
            // continue on
            if (!standard && !cyclic)
            {
                continue;
            }

            RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_DATAFLOW, &dataFlow, sizeof(KSPIN_DATAFLOW)));

            // for render (MIDIOut) we need  KSPIN_DATAFLOW_IN
            if (KSPIN_DATAFLOW_IN == dataFlow)
            {
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].FilterName =
                    wil::make_cotaskmem_string_nothrow(deviceId);
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].PinId = i;
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].Cyclic = cyclic;
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].Standard = standard;
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].UMP = UMP;
                m_AvailableMidiOutPinCount++;
            }
            else if (KSPIN_DATAFLOW_OUT == dataFlow)
            {
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].FilterName =
                    wil::make_cotaskmem_string_nothrow(deviceId);
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].PinId = i;
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].Cyclic = cyclic;
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].Standard = standard;
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].UMP = UMP;
                m_AvailableMidiInPinCount++;
            }
            else
            {
                continue;
            }
        }
        return S_OK;
    });

    auto deviceStoppedHandler = Callback<CStoppedHandler>([&](IDeviceWatcher* , IInspectable*) -> HRESULT {
        enumerationCompleted.SetEvent();
        return S_OK;
    });

    auto enumerationCompletedHandler = Callback<CEnumerationCompletedHandler>([&](IDeviceWatcher* , IInspectable*) -> HRESULT {
        enumerationCompleted.SetEvent();
        return S_OK;
    });

    RETURN_IF_FAILED(watcher->add_Added(deviceAddedHandler.Get(), &tokenAdded));
    RETURN_IF_FAILED(watcher->add_Stopped(deviceStoppedHandler.Get(), &tokenStopped));
    RETURN_IF_FAILED(watcher->add_EnumerationCompleted(deviceStoppedHandler.Get(), &tokenEnumerationCompleted));

    RETURN_IF_FAILED(watcher->Start());

    enumerationCompleted.wait();

    watcher->remove_EnumerationCompleted(tokenEnumerationCompleted);
    watcher->remove_Stopped(tokenStopped);
    watcher->remove_Added(tokenAdded);

    return S_OK;
}

HRESULT
KSMidiDeviceEnum::Cleanup()
{
    return S_OK;
}

