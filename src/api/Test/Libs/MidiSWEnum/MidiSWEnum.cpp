// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <avrt.h>
#include <Devpkey.h>
#include <mmdeviceapi.h>
#include "MidiDefs.h"
#include "MidiDeviceManagerInterface.h"
#include "MidiSwEnum.h"

#include "Midi2MidiSrvAbstraction.h"

using namespace winrt::Windows::Devices::Enumeration;

MidiSWDeviceEnum::MidiSWDeviceEnum()
{
}

MidiSWDeviceEnum::~MidiSWDeviceEnum()
{
    Cleanup();
}

// enumerate both UMP and MidiOne endpoints for testing
#define MIDI_DEVICE_SELECTOR \
    L"(System.Devices.InterfaceClassGuid:=\"{AE174174-6396-4DEE-AC9E-1E9C6F403230}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{3705DC2B-17A7-4452-98CE-BF12C6F48A0B}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}\")"\
    L"AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"

HRESULT
MidiSWDeviceEnum::EnumerateDevices(_In_ GUID requestedAbstraction)
{
    winrt::hstring deviceSelector(MIDI_DEVICE_SELECTOR);
    wil::unique_event enumerationCompleted{wil::EventOptions::None};

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_AbstractionLayer));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));

    auto watcher = DeviceInformation::CreateWatcher(deviceSelector, additionalProperties);

    auto deviceAddedHandler = watcher.Added(winrt::auto_revoke, [&](DeviceWatcher watcher, DeviceInformation device)
    {
        auto name = device.Name();
        auto instanceId = device.Id();
        winrt::guid abstractionGuid;

        auto prop = device.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_AbstractionLayer));
        if (prop)
        {
            abstractionGuid = winrt::unbox_value<winrt::guid>(prop);

            // if it's not the midisrv abstraction, and it's not the requested abstraction,
            // filter it.
            if (requestedAbstraction != __uuidof(Midi2MidiSrvAbstraction) &&
                (GUID)abstractionGuid != requestedAbstraction)
            {
                return S_OK;
            }
        }
        else
        {
            // TODO: SWD's created by AudioEndpointBuilder lack an abstraction guid entirely,
            // we don't currently know how to use these endpoints. Skip for now.
            return S_OK;
        }

        prop = device.Properties().Lookup(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));
        auto interfaceClass = winrt::unbox_value<winrt::guid>(prop);
        
        std::unique_ptr<MIDIU_DEVICE> midiDevice = std::make_unique<MIDIU_DEVICE>();
        
        RETURN_IF_NULL_ALLOC(midiDevice);

        midiDevice->AbstractionLayer = abstractionGuid;
        midiDevice->InterfaceClass = interfaceClass;
        midiDevice->InstanceId = instanceId;
        midiDevice->Name = name;

        if (winrt::guid(DEVINTERFACE_MIDI_INPUT) == interfaceClass)
        {
            midiDevice->Flow = MidiFlowIn;
            midiDevice->MidiOne = TRUE;
        }
        else if (winrt::guid(DEVINTERFACE_MIDI_OUTPUT) == interfaceClass)
        {
            midiDevice->Flow = MidiFlowOut;
            midiDevice->MidiOne = TRUE;
        }
        else if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_INPUT) == interfaceClass)
        {
            midiDevice->Flow = MidiFlowIn;
        }
        else if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_OUTPUT) == interfaceClass)
        {
            midiDevice->Flow = MidiFlowOut;
        }
        else if (winrt::guid(DEVINTERFACE_UNIVERSALMIDIPACKET_BIDI) == interfaceClass)
        {
            midiDevice->Flow = MidiFlowBidirectional;
        }
        else
        {
            RETURN_IF_FAILED(E_UNEXPECTED);
        }

        m_AvailableMidiDevices.push_back(std::move(midiDevice));

        return S_OK;
    });

    auto deviceStoppedHandler = watcher.Stopped(winrt::auto_revoke, [&](DeviceWatcher, winrt::Windows::Foundation::IInspectable)
    {
        enumerationCompleted.SetEvent();
        return S_OK;
    });

    auto enumerationCompletedHandler = watcher.EnumerationCompleted(winrt::auto_revoke, [&](DeviceWatcher , winrt::Windows::Foundation::IInspectable)
    {
        enumerationCompleted.SetEvent();
        return S_OK;
    });

    watcher.Start();
    enumerationCompleted.wait();
    watcher.Stop();
    enumerationCompleted.wait();

    deviceAddedHandler.revoke();
    deviceStoppedHandler.revoke();
    enumerationCompletedHandler.revoke();

    return S_OK;
}

_Use_decl_annotations_
UINT
MidiSWDeviceEnum::GetNumMidiDevices(MidiFlow Flow, BOOL MidiOne)
{
    UINT numDevices = 0;

    for (auto const& MidiDevice : m_AvailableMidiDevices)
    {
        if (MidiDevice->MidiOne == MidiOne &&
            MidiDevice->Flow == Flow)
        {
            numDevices++;
        }
    }
    return numDevices;
}

_Use_decl_annotations_
std::wstring
MidiSWDeviceEnum::GetMidiInstanceId(UINT Index, MidiFlow Flow, BOOL MidiOne)
{
    std::wstring midiDevice;

    for (auto const& MidiDevice : m_AvailableMidiDevices)
    {
        if (MidiDevice->MidiOne == MidiOne &&
            MidiDevice->Flow == Flow)
        {
            if (Index == 0)
            {
                midiDevice = MidiDevice->InstanceId;
                break;
            }
            Index--;
        }
    }

    return midiDevice;
}

HRESULT
MidiSWDeviceEnum::Cleanup()
{
    return S_OK;
}

