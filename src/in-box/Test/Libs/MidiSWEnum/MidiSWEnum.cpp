// Copyright (c) Microsoft Corporation. All rights reserved.

#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <functional>

#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <avrt.h>
#include <Devpkey.h>
#include <mmdeviceapi.h>
#include "MidiDefs.h"
#include "WindowsMidiServices.h"
#include "MidiSwEnum.h"

#include "wstring_util.h"
#include "swd_helpers.h"

#include "Midi2MidiSrvTransport.h"

using namespace winrt::Windows::Devices::Enumeration;

// enumerate both UMP and MidiOne endpoints for testing
#define MIDI_DEVICE_SELECTOR \
    L"(System.Devices.InterfaceClassGuid:=\"{AE174174-6396-4DEE-AC9E-1E9C6F403230}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{3705DC2B-17A7-4452-98CE-BF12C6F48A0B}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}\""\
    L"OR System.Devices.InterfaceClassGuid:=\"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}\")"\
    L"AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"

_Use_decl_annotations_
HRESULT
MidiSWDeviceEnum::EnumerateDevices(
    std::vector<std::unique_ptr<MIDIU_DEVICE>>& devices,
    std::function<bool(PMIDIU_DEVICE)>&& predicate
)
{
    winrt::hstring deviceSelector(MIDI_DEVICE_SELECTOR);

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(STRING_PKEY_MIDI_TransportLayer);
    additionalProperties.Append(L"System.Devices.InterfaceClassGuid");
    additionalProperties.Append(STRING_PKEY_MIDI_SupportedDataFormats);
    additionalProperties.Append(STRING_PKEY_MIDI_DriverDeviceInterface);
    additionalProperties.Append(STRING_PKEY_MIDI_ServiceAssignedPortNumber);

    auto deviceList = DeviceInformation::FindAllAsync(deviceSelector, additionalProperties).get();

    for (auto const& device : deviceList)
    {
        auto name = device.Name();
        auto deviceId = device.Id();
        winrt::guid transportGuid;
        auto additionalProperties2 = winrt::single_threaded_vector<winrt::hstring>();
        MidiDataFormats supportedDataFormats { MidiDataFormats_Invalid };

        auto prop = device.Properties().Lookup(STRING_PKEY_MIDI_TransportLayer);
        if (prop)
        {
            transportGuid = winrt::unbox_value<winrt::guid>(prop);
        }
        else
        {
            // TODO: SWD's created by AudioEndpointBuilder lack an transport guid entirely,
            // we don't currently know how to use these endpoints. Skip for now.
            continue;
        }

        prop = device.Properties().Lookup(STRING_PKEY_MIDI_SupportedDataFormats);
        if (!prop)
        {
            continue;
        }
        supportedDataFormats = (MidiDataFormats) winrt::unbox_value<uint8_t>(prop);

        prop = device.Properties().Lookup(L"System.Devices.InterfaceClassGuid");
        if (!prop)
        {
            continue;
        }
        auto interfaceClass = winrt::unbox_value<winrt::guid>(prop);

        prop = device.Properties().Lookup(L"System.Devices.DeviceInstanceId");
        if (!prop)
        {
            continue;
        }
        std::wstring deviceInstanceId = winrt::unbox_value<winrt::hstring>(prop).c_str();
        
        std::wstring driverDeviceInterfaceId{};
        prop = device.Properties().Lookup(STRING_PKEY_MIDI_DriverDeviceInterface);
        if (prop)
        {
            driverDeviceInterfaceId = winrt::unbox_value<winrt::hstring>(prop).c_str();
        }

        UINT32 serviceAssignedPortNumber = 0;
        prop = device.Properties().Lookup(STRING_PKEY_MIDI_ServiceAssignedPortNumber);
        if (prop)
        {
            serviceAssignedPortNumber = winrt::unbox_value<uint32_t>(prop);
        }
        

        std::unique_ptr<MIDIU_DEVICE> midiDevice = std::make_unique<MIDIU_DEVICE>();
        if (!midiDevice)
        {
            continue;
        }

        midiDevice->TransportLayer = transportGuid;
        midiDevice->InterfaceClass = interfaceClass;
        midiDevice->DeviceId = deviceId;
        midiDevice->DeviceInstanceId = WindowsMidiServicesInternal::NormalizeDeviceInstanceIdWStringCopy(deviceInstanceId);
        midiDevice->DriverDeviceInterfaceId = driverDeviceInterfaceId.c_str();
        midiDevice->Name = name;
        midiDevice->SupportedDataFormats = supportedDataFormats;

        if (winrt::guid(DEVINTERFACE_MIDI_INPUT) == interfaceClass)
        {
            // service assigned port numbers skip index 0, going from 1 to N,
            // winmm goes from 0 to N-1, so we need to subtract 1 from the midi in
            // port number to get the winmm port number.
            midiDevice->WinmmPortNumber = serviceAssignedPortNumber - 1;
            midiDevice->Flow = MidiFlowIn;
            midiDevice->MidiOne = TRUE;
        }
        else if (winrt::guid(DEVINTERFACE_MIDI_OUTPUT) == interfaceClass)
        {
            // service assigned port numbers skip index 0, going from 1 to N,
            // winmm port numbers go from 0 to N-1, but the midi synth
            // is always at port 0 for midi out. The result of the -1 due to the
            // indexing difference, and the +1 for the synth means that
            // winmmPortNumber == serviceAssignedPortNumber for midi out.
            midiDevice->WinmmPortNumber = serviceAssignedPortNumber;
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
            continue;
        }

        additionalProperties2.Append(L"System.Devices.Parent");

        auto parentDeviceInfo = DeviceInformation::CreateFromIdAsync(deviceInstanceId,
        additionalProperties2, winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();

        prop = parentDeviceInfo.Properties().Lookup(L"System.Devices.Parent");
        RETURN_HR_IF_NULL(E_INVALIDARG, prop);
        std::wstring parentDeviceInstanceId = winrt::unbox_value<winrt::hstring>(prop).c_str();
        midiDevice->ParentDeviceInstanceId = WindowsMidiServicesInternal::NormalizeDeviceInstanceIdWStringCopy(parentDeviceInstanceId);

        if (predicate(midiDevice.get()))
        {
            devices.push_back(std::move(midiDevice));
        }
    }

    return S_OK;
}

