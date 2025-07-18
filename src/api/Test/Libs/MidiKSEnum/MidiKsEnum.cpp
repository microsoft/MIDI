// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>
#include <cguid.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <ks.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "MidiKsEnum.h"
#include "KsHandleWrapper.h"

using namespace winrt::Windows::Devices::Enumeration;

KSMidiDeviceEnum::KSMidiDeviceEnum()
{
}

KSMidiDeviceEnum::~KSMidiDeviceEnum()
{
    Shutdown();
}

HRESULT
KSMidiDeviceEnum::EnumerateFilters()
{
    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    auto deviceList = DeviceInformation::FindAllAsync(deviceSelector).get();

    for (auto const& device : deviceList)
    {
        PCWSTR deviceId = device.Id().c_str();
        ULONG cPins = 0;

        // Wrapper opens the handle internally.
        KsHandleWrapper deviceHandleWrapper(deviceId);

        if (FAILED(deviceHandleWrapper.Open()))
        {
            continue;
        }

        // Using lamba function to prevent handle from dissapearing when being used. 
        HRESULT hr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
            return PinPropertySimple(h, 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins));
        });

        if (FAILED(hr))
        {
            continue;
        }

        for (UINT i = 0; i < cPins; i++)
        {
            KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
            MidiTransport transportCapability { MidiTransport_Invalid };
            KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;

            hr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return PinPropertySimple(h, i, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION));
            });

            if (FAILED(hr))
            {
                continue;
            }

            // The external connector pin representing the phsyical connection
            // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
            // have a valid communication. Skip connector pins.
            if (KSPIN_COMMUNICATION_NONE == communication)
            {
                continue;
            }

            // Duplicate the handle to safely pass it to another component or store it.
            wil::unique_handle handleDupe(deviceHandleWrapper.GetHandle());
            RETURN_IF_NULL_ALLOC(handleDupe);

            KsHandleWrapper m_PinHandleWrapper1(deviceId, i, MidiTransport_CyclicUMP, handleDupe.get());

            // Cyclic buffering with UMP messages, MIDI 2 driver and peripheral.
            if (SUCCEEDED(m_PinHandleWrapper1.Open()))
            {
                transportCapability = (MidiTransport )((DWORD) transportCapability |  (DWORD) MidiTransport_CyclicUMP);
                m_PinHandleWrapper1.Close();
            }

            KsHandleWrapper m_PinHandleWrapper2(deviceId, i, MidiTransport_CyclicByteStream, handleDupe.get());

            // Cyclic buffering, but legacy bytestream, potentially useful for moving
            // MIDI 1 messages efficiently to and from a new driver communicating with
            // a legacy peripheral. Only useful for a driver that also supports cyclic
            // UMP
            if (SUCCEEDED(m_PinHandleWrapper2.Open()))
            {
                transportCapability = (MidiTransport )((DWORD) transportCapability |  (DWORD) MidiTransport_CyclicByteStream);
                m_PinHandleWrapper2.Close();
            }

            KsHandleWrapper m_PinHandleWrapper3(deviceId, i, MidiTransport_StandardByteStream, handleDupe.get());

            // Standard buffering with bytesteam, a MIDI 1 KS driver.
            if (SUCCEEDED(m_PinHandleWrapper3.Open()))
            {
                transportCapability = (MidiTransport )((DWORD) transportCapability |  (DWORD) MidiTransport_StandardByteStream);
                m_PinHandleWrapper3.Close();
            }

            // if this pin supports nothing, then it's not a streaming pin.
            if (0 == transportCapability)
            {
                continue;
            }

            HRESULT lambdaHr = deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
                return PinPropertySimple(h, i, KSPROPSETID_Pin, KSPROPERTY_PIN_DATAFLOW, &dataFlow, sizeof(KSPIN_DATAFLOW));
            });

            if (FAILED(lambdaHr))
            {
                continue;
            }

            // for render (MIDIOut) we need  KSPIN_DATAFLOW_IN
            if (KSPIN_DATAFLOW_IN == dataFlow)
            {
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].FilterName =
                    wil::make_cotaskmem_string_nothrow(deviceId);
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].PinId = i;
                m_AvailableMidiOutPins[m_AvailableMidiOutPinCount].TransportCapability = transportCapability;
                m_AvailableMidiOutPinCount++;
            }
            else if (KSPIN_DATAFLOW_OUT == dataFlow)
            {
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].FilterName =
                    wil::make_cotaskmem_string_nothrow(deviceId);
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].PinId = i;
                m_AvailableMidiInPins[m_AvailableMidiInPinCount].TransportCapability = transportCapability;
                m_AvailableMidiInPinCount++;
            }
            else
            {
                continue;
            }
        }
    }

    return S_OK;
}

HRESULT
KSMidiDeviceEnum::Shutdown()
{
    return S_OK;
}

