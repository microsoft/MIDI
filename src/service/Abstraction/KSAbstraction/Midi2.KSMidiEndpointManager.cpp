// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

using namespace wil;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

#define CREATE_KS_BIDI_SWDS
// build in, out, and bidi, for current testing.
//#define BIDI_REPLACES_INOUT_SWDS

#define MAX_DEVICE_ID_LEN 200 // size in chars

GUID KsAbstractionLayerGUID = __uuidof(Midi2KSAbstraction);

_Use_decl_annotations_
HRESULT
CMidi2KSMidiEndpointManager::Initialize(
    IUnknown* midiDeviceManager
)
{
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(winrt::to_hstring(L"System.Devices.FriendlyName"));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.DeviceDescription1"));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.DeviceDescription2"));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.Parent"));

    m_Watcher = DeviceInformation::CreateWatcher(deviceSelector, additionalProperties);

    auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSMidiEndpointManager::OnDeviceAdded);
    auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSMidiEndpointManager::OnDeviceRemoved);
    auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSMidiEndpointManager::OnDeviceUpdated);
    auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSMidiEndpointManager::OnDeviceStopped);
    auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSMidiEndpointManager::OnEnumerationCompleted);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_Watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_Watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

    m_Watcher.Start();
    
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnDeviceAdded(DeviceWatcher watcher, DeviceInformation device)
{
    wil::unique_handle hFilter;
    std::wstring deviceName;
    std::wstring deviceId;
    std::wstring deviceInstanceId;
    std::hash<std::wstring> hasher;
    std::wstring hash;
    ULONG cPins{ 0 };
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    auto properties = device.Properties();

    // retrieve the device instance id from the DeviceInformation property store
    auto prop = properties.Lookup(L"System.Devices.DeviceInstanceId");
    deviceInstanceId = winrt::unbox_value<winrt::hstring>(prop).c_str();
    deviceId = device.Id().c_str();

    // DeviceInterfaces often lack names, so the above device.Name() is likely the name
    // of the machine. Grab the parent device friendly name and use that instead.
    //
    // TODO: use a provided name from a json first, if it is available for this device+filter+pin.
    // second, can we retrieve the localized part name for the interface using KS? That would likely require
    // calling a property to get the name GUID from the filter, and then using that guid to locate the string
    // from the KS friendly name registry path.
    auto parentDeviceInfo = DeviceInformation::CreateFromIdAsync(deviceInstanceId,
        additionalProperties,winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();
    deviceName = parentDeviceInfo.Name();

    hash = std::to_wstring(hasher(deviceId));

    std::vector<std::unique_ptr<MIDI_PIN_INFO>> newMidiPins;

    // instantiate the interface
    RETURN_IF_FAILED(FilterInstantiate(deviceId.c_str(), &hFilter));
    RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins)));

    for (UINT i = 0; i < cPins; i++)
    {
        wil::unique_handle hPin;
        KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
        BOOL cyclicUmp = FALSE;
        BOOL standardUmp = FALSE;
        BOOL midiOne = FALSE;
        KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;

        RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION)));

        // The external connector pin representing the phsyical connection
        // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
        // have a valid communication. Skip connector pins.
        if (KSPIN_COMMUNICATION_NONE == communication)
        {
            continue;
        }

        // attempt to instantiate using standard streaming for UMP buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, FALSE, TRUE, &hPin)))
        {
            standardUmp = TRUE;
            hPin.reset();
        }

        // attempt to instantiate using cyclic streaming for UMP buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, TRUE, TRUE, &hPin)))
        {
            cyclicUmp = TRUE;
            hPin.reset();
        }

        // attempt to instantiate using standard streaming for midiOne buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, FALSE, FALSE, &hPin)))
        {
            midiOne = TRUE;
            hPin.reset();
        }

        // if this pin supports nothing, then it's not a streaming pin,
        // continue on
        if (!standardUmp && !cyclicUmp && !midiOne)
        {
            continue;
        }

        RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_DATAFLOW, &dataFlow, sizeof(KSPIN_DATAFLOW)));

        std::unique_ptr<MIDI_PIN_INFO> midiPin = std::make_unique<MIDI_PIN_INFO>();

        RETURN_IF_NULL_ALLOC(midiPin);

        // for render (MIDIOut) we need KSPIN_DATAFLOW_IN
        midiPin->Flow = (KSPIN_DATAFLOW_IN == dataFlow) ? MidiFlowOut : MidiFlowIn;
        midiPin->PinId = i;
        midiPin->CyclicUmp = cyclicUmp;
        midiPin->StandardUmp = standardUmp;
        midiPin->MidiOne = midiOne;
        midiPin->Name = deviceName;
        midiPin->Id = deviceId;
        midiPin->ParentInstanceId = deviceInstanceId;


        midiPin->InstanceId = L"MIDIU_KS_";
        midiPin->InstanceId += (midiPin->Flow == MidiFlowOut) ? L"OUT_" : L"IN_";
        midiPin->InstanceId += hash;
        midiPin->InstanceId += L"_PIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinId);

        newMidiPins.push_back(std::move(midiPin));
    }

#ifdef CREATE_KS_BIDI_SWDS
    // there must be exactly two pins on this filter, midi in, and midi out,
    // and they must have the exact same capabilities
    if (newMidiPins.size() == 2 &&
        newMidiPins[0]->Flow != newMidiPins[1]->Flow &&
        newMidiPins[0]->CyclicUmp == newMidiPins[1]->CyclicUmp &&
        newMidiPins[1]->StandardUmp == newMidiPins[1]->StandardUmp &&
        newMidiPins[1]->MidiOne == newMidiPins[1]->MidiOne)
    {
        auto midiInPinIndex = newMidiPins[0]->Flow == MidiFlowIn?0:1;
        auto midiOutPinIndex = newMidiPins[0]->Flow == MidiFlowOut?0:1;

        // create a new bidi entry
        std::unique_ptr<MIDI_PIN_INFO> midiPin = std::make_unique<MIDI_PIN_INFO>();

        RETURN_IF_NULL_ALLOC(midiPin);

        midiPin->PinId = newMidiPins[midiOutPinIndex]->PinId;
        midiPin->PinIdIn = newMidiPins[midiInPinIndex]->PinId;
        midiPin->CyclicUmp = newMidiPins[midiOutPinIndex]->CyclicUmp;
        midiPin->StandardUmp = newMidiPins[midiOutPinIndex]->StandardUmp;
        midiPin->MidiOne = newMidiPins[midiOutPinIndex]->MidiOne;
        midiPin->Name = deviceName;
        midiPin->Id = deviceId;
        midiPin->ParentInstanceId = deviceInstanceId;
        midiPin->Flow = MidiFlowBidirectional;

        midiPin->InstanceId = L"MIDIU_KS_BIDI_";
        midiPin->InstanceId += hash;
        midiPin->InstanceId += L"_OUTPIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinId);
        midiPin->InstanceId += L"_INPIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinIdIn);

#ifdef BIDI_REPLACES_INOUT_SWDS
        // replace the existing entries with this one bidi entry
        newMidiPins.clear();
#else
        // don't want to double the legacy SWD's, so if we are
        // creating both in and out swd's, no need to duplicate
        // bidirectional as well.
        midiPin->CreateUMPOnly = TRUE;
#endif
        newMidiPins.push_back(std::move(midiPin));
    }
#endif

    //in the added callback we're going to go ahead and create the SWD's:
    for (auto const& MidiPin : newMidiPins)
    {
        DEVPROP_BOOLEAN supportsMidiOne = (MidiPin->MidiOne)?DEVPROP_TRUE:DEVPROP_FALSE;
        DEVPROP_BOOLEAN supportsUmp = (MidiPin->CyclicUmp || MidiPin->StandardUmp)?DEVPROP_TRUE:DEVPROP_FALSE;
        DEVPROP_BOOLEAN supportsCyclic = (MidiPin->CyclicUmp)?DEVPROP_TRUE:DEVPROP_FALSE;

        // 6 items added at initialization, up to 2 additional items added below, for a total of 8.
        ULONG interfacePropertyCount{ 6 };
        DEVPROPERTY interfaceDevProperties[8] = {
            { {DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((MidiPin->Name.length() + 1) * sizeof(WCHAR)), (PVOID)MidiPin->Name.c_str() },
            { {DEVPKEY_KsMidiPort_KsFilterInterfaceId, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((MidiPin->Id.length() + 1) * sizeof(WCHAR)), (PVOID)MidiPin->Id.c_str() },
            { {PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&KsAbstractionLayerGUID },
            { { DEVPKEY_KsMidiPort_SupportsMidiOneFormat, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsMidiOne)), (PVOID)&supportsMidiOne },
            { { DEVPKEY_KsMidiPort_SupportsUMPFormat, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsUmp)), (PVOID)&supportsUmp },
            { { DEVPKEY_KsMidiPort_SupportsLooped, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsCyclic)), (PVOID)&supportsCyclic },
            { 0 } // set the remainder to 0
        };

        // Bidirectional uses a different property for the in and out pins, since we currently require two separate ones.
        if (MidiPin->Flow == MidiFlowBidirectional)
        {
            interfaceDevProperties[interfacePropertyCount++] = { { DEVPKEY_KsMidiPort_OutPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) };
            interfaceDevProperties[interfacePropertyCount++] = { { DEVPKEY_KsMidiPort_InPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinIdIn) };
        }
        else
        {
            // In and out have only a single pin id to create.
            interfaceDevProperties[interfacePropertyCount++] = { { DEVPKEY_KsMidiPort_KsPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) };
        }

        DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;

        DEVPROPERTY deviceDevProperties[] = {
            {{DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue}
        };

        SW_DEVICE_CREATE_INFO createInfo = {};
        createInfo.cbSize = sizeof(createInfo);

        createInfo.pszInstanceId = MidiPin->InstanceId.c_str();
        createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
        createInfo.pszDeviceDescription = MidiPin->Name.c_str();

        LOG_IF_FAILED(m_MidiDeviceManager->ActivateEndpoint(MidiPin->ParentInstanceId.c_str(), MidiPin->CreateUMPOnly, MidiPin->Flow, interfacePropertyCount, ARRAYSIZE(deviceDevProperties), (PVOID)interfaceDevProperties, (PVOID)deviceDevProperties, (PVOID)&createInfo));
    }


    while (newMidiPins.size() > 0)
    {
        m_AvailableMidiPins.push_back(std::move(newMidiPins.back()));
        newMidiPins.pop_back();
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate device)
{

    // the interface is no longer active, search through our m_AvailableMidiPins to identify
    // every entry with this filter interface id, and remove the SWD and remove the pin(s) from
    // the m_AvailableMidiPins list.
    do
    {
        auto item = std::find_if(m_AvailableMidiPins.begin(), m_AvailableMidiPins.end(), [&](const std::unique_ptr<MIDI_PIN_INFO>& Pin)
        {
            // if this interface id already activated, then we cannot activate/create a second time,
            if (device.Id() == Pin->Id)
            {
                return true;
            }

            return false;
        });

        if (item == m_AvailableMidiPins.end())
        {
            break;
        }

        // notify the device manager using the InstanceId for this midi device
        LOG_IF_FAILED(m_MidiDeviceManager->RemoveEndpoint(item->get()->InstanceId.c_str()));

        // remove the MIDI_PIN_INFO from the list
        m_AvailableMidiPins.erase(item);
    }
    while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate)
{
    //see this function for info on the IDeviceInformationUpdate object: https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices#enumerate-and-watch-devices
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnDeviceStopped(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnEnumerationCompleted(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

HRESULT
CMidi2KSMidiEndpointManager::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    m_EnumerationCompleted.wait();
    m_Watcher.Stop();
    m_EnumerationCompleted.wait();
    m_DeviceAdded.revoke();
    m_DeviceRemoved.revoke();
    m_DeviceUpdated.revoke();
    m_DeviceStopped.revoke();
    m_DeviceEnumerationCompleted.revoke();

    return S_OK;
}
