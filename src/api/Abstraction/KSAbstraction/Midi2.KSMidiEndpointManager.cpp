// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "midi2.ksabstraction.h"

using namespace wil;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

// if this is defined, then KSMidiEndpointManager will publish a BiDi endpoint
// for pairs of midi in & out endpoints on the same filter.
// Filters which do not have a a single pair of midi in and out,
// separate midi in and out SWD's will always be created.
#define CREATE_KS_BIDI_SWDS

// If this is defined, we will skip building midi in and midi out
// SWD's for endpoints where BIDI SWD's are created.
// Otherwise, MidiIn, Out, and BiDi will be created. Creating all 3
// is OK for unit testing one at a time, however is not valid for
// normal usage because MidiIn and MidiOut use the same pins as 
// MidiBidi, so only MidiIn and MidiOut or MidiBidi can be used,
// never all 3 at the same time.
//#define BIDI_REPLACES_INOUT_SWDS

_Use_decl_annotations_
HRESULT
CMidi2KSMidiEndpointManager::Initialize(
    IUnknown* midiDeviceManager,
    IUnknown* midiEndpointProtocolManager,
    LPCWSTR configurationJson
)
{
    OutputDebugString(L"\n" __FUNCTION__);

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));

    try
    {
        if (configurationJson != nullptr)
        {
            winrt::hstring jsonString{ configurationJson };

            if (jsonString != L"")
            {
                if (json::JsonObject::TryParse(configurationJson, m_jsonObject))
                {
                    // worked
                }
            }
        }
    }
    catch (...)
    {
        m_jsonObject = nullptr;
    }


    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True");

    m_Watcher = DeviceInformation::CreateWatcher(deviceSelector);

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

    std::wstring mnemonic(TRANSPORT_MNEMONIC);

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    auto properties = device.Properties();

    // retrieve the device instance id from the DeviceInformation property store
    auto prop = properties.Lookup(winrt::to_hstring(L"System.Devices.DeviceInstanceId"));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
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
        MidiTransport transportCapability { MidiTransport_Invalid };
        MidiDataFormat dataFormatCapability { MidiDataFormat_Invalid };
        KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;
        GUID nativeDataFormat {0};
        std::unique_ptr<BYTE> groupTerminalBlockData;
        ULONG groupTerminalBlockDataSize {0};

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
        // NOTE: this has been for bring up performance comparison testing only, and will be removed.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_StandardByteStream, &hPin)))
        {
            transportCapability = ( MidiTransport )((DWORD) transportCapability | (DWORD) MidiTransport_StandardByteStream);
            dataFormatCapability = ( MidiDataFormat ) ((DWORD) dataFormatCapability | (DWORD) MidiDataFormat_ByteStream);
            hPin.reset();
        }

        // attempt to instantiate using cyclic streaming for UMP buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_CyclicUMP, &hPin)))
        {
            transportCapability = (MidiTransport )((DWORD) transportCapability |  (DWORD) MidiTransport_CyclicUMP);
            dataFormatCapability = ( MidiDataFormat ) ((DWORD) dataFormatCapability | (DWORD) MidiDataFormat_UMP);

            LOG_IF_FAILED_WITH_EXPECTED(PinPropertySimple(hPin.get(), 
                                            i, 
                                            KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
                                            KSPROPERTY_MIDI2_NATIVEDATAFORMAT, 
                                            &nativeDataFormat, 
                                            sizeof(nativeDataFormat)),
                                            HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            LOG_IF_FAILED_WITH_EXPECTED(PinPropertyAllocate(hPin.get(), 
                                            i, 
                                            KSPROPSETID_MIDI2_ENDPOINT_INFORMATION, 
                                            KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,
                                            (PVOID *)&groupTerminalBlockData, 
                                            &groupTerminalBlockDataSize),
                                            HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            hPin.reset();
        }

        // attempt to instantiate using standard streaming for midiOne buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_CyclicByteStream, &hPin)))
        {
            transportCapability = (MidiTransport )((DWORD) transportCapability |  (DWORD) MidiTransport_CyclicByteStream);
            dataFormatCapability = ( MidiDataFormat ) ((DWORD) dataFormatCapability | (DWORD) MidiDataFormat_ByteStream);
            hPin.reset();
        }

        // if this pin supports nothing, then it's not a streaming pin,
        // continue on
        if (0 == transportCapability)
        {
            continue;
        }

        RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_DATAFLOW, &dataFlow, sizeof(KSPIN_DATAFLOW)));

        std::unique_ptr<MIDI_PIN_INFO> midiPin = std::make_unique<MIDI_PIN_INFO>();

        RETURN_IF_NULL_ALLOC(midiPin);

        // for render (MIDIOut) we need KSPIN_DATAFLOW_IN
        midiPin->Flow = (KSPIN_DATAFLOW_IN == dataFlow) ? MidiFlowOut : MidiFlowIn;
        midiPin->PinId = i;
        midiPin->TransportCapability = transportCapability;
        midiPin->DataFormatCapability = dataFormatCapability;
        midiPin->Name = deviceName;
        midiPin->Id = deviceId;
        midiPin->ParentInstanceId = deviceInstanceId;

        if (midiPin->Flow == MidiFlowOut)
        {
           midiPin->GroupTerminalBlockDataOut = std::move(groupTerminalBlockData);
           midiPin->GroupTerminalBlockDataSizeOut = groupTerminalBlockDataSize;
        }
        else
        {
            midiPin->GroupTerminalBlockDataIn = std::move(groupTerminalBlockData);
            midiPin->GroupTerminalBlockDataSizeIn = groupTerminalBlockDataSize;
        }
        midiPin->NativeDataFormat = nativeDataFormat;

        midiPin->InstanceId = L"MIDIU_KS_";
        midiPin->InstanceId += (midiPin->Flow == MidiFlowOut) ? L"OUT_" : L"IN_";
        midiPin->InstanceId += hash;
        midiPin->InstanceId += L"_PIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinId);

        newMidiPins.push_back(std::move(midiPin));
    }

#ifdef CREATE_KS_BIDI_SWDS
    // TODO: This logic needs to change because we want ALL KS devices to show up
    // as bidi, even if they have only a single pin
    // https://github.com/microsoft/MIDI/issues/184
     
     
    // there must be exactly two pins on this filter, midi in, and midi out,
    // and they must have the exact same capabilities
    if (newMidiPins.size() == 2 &&
        newMidiPins[0]->Flow != newMidiPins[1]->Flow &&
        newMidiPins[0]->TransportCapability == newMidiPins[1]->TransportCapability)
    {
        auto midiInPinIndex = newMidiPins[0]->Flow == MidiFlowIn?0:1;
        auto midiOutPinIndex = newMidiPins[0]->Flow == MidiFlowOut?0:1;

        // create a new bidi entry
        std::unique_ptr<MIDI_PIN_INFO> midiPin = std::make_unique<MIDI_PIN_INFO>();

        RETURN_IF_NULL_ALLOC(midiPin);

        midiPin->PinId = newMidiPins[midiOutPinIndex]->PinId;
        midiPin->PinIdIn = newMidiPins[midiInPinIndex]->PinId;
        midiPin->TransportCapability = newMidiPins[midiInPinIndex]->TransportCapability;
        midiPin->DataFormatCapability = newMidiPins[midiInPinIndex]->DataFormatCapability;
        midiPin->Name = deviceName;
        midiPin->Id = deviceId;
        midiPin->ParentInstanceId = deviceInstanceId;
        if (newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSizeOut > 0)
        {
            std::unique_ptr<BYTE> groupTerminalBlockData(new (std::nothrow) BYTE[newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSizeOut]);
            RETURN_IF_NULL_ALLOC(groupTerminalBlockData);
            memcpy(groupTerminalBlockData.get(), newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataOut.get(), newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSizeOut);
            midiPin->GroupTerminalBlockDataOut = std::move(groupTerminalBlockData);
            midiPin->GroupTerminalBlockDataSizeOut = newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSizeOut;
        }

        if (newMidiPins[midiInPinIndex]->GroupTerminalBlockDataSizeIn > 0)
        {
            std::unique_ptr<BYTE> groupTerminalBlockData(new (std::nothrow) BYTE[newMidiPins[midiInPinIndex]->GroupTerminalBlockDataSizeIn]);
            RETURN_IF_NULL_ALLOC(groupTerminalBlockData);
            memcpy(groupTerminalBlockData.get(), newMidiPins[midiInPinIndex]->GroupTerminalBlockDataIn.get(), newMidiPins[midiInPinIndex]->GroupTerminalBlockDataSizeIn);
            midiPin->GroupTerminalBlockDataIn = std::move(groupTerminalBlockData);
            midiPin->GroupTerminalBlockDataSizeIn = newMidiPins[midiInPinIndex]->GroupTerminalBlockDataSizeIn;
        }

        midiPin->NativeDataFormat = newMidiPins[midiOutPinIndex]->NativeDataFormat;
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
        GUID KsAbstractionLayerGUID = __uuidof(Midi2KSAbstraction);
        DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
        DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

        std::vector<DEVPROPERTY> interfaceDevProperties;
        std::vector<DEVPROPERTY> deviceDevProperties;

        interfaceDevProperties.push_back({ {DEVPKEY_DeviceInterface_FriendlyName, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((MidiPin->Name.length() + 1) * sizeof(WCHAR)), (PVOID)MidiPin->Name.c_str() });
        interfaceDevProperties.push_back({ {DEVPKEY_KsMidiPort_KsFilterInterfaceId, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((MidiPin->Id.length() + 1) * sizeof(WCHAR)), (PVOID)MidiPin->Id.c_str() });
        interfaceDevProperties.push_back({ {PKEY_MIDI_AbstractionLayer, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_GUID, static_cast<ULONG>(sizeof(GUID)), (PVOID)&KsAbstractionLayerGUID });
        interfaceDevProperties.push_back({ {DEVPKEY_KsTransport, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), (PVOID)&MidiPin->TransportCapability });
        interfaceDevProperties.push_back({ {PKEY_MIDI_SupportedDataFormats, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), (PVOID)&MidiPin->DataFormatCapability });
        interfaceDevProperties.push_back({ {PKEY_MIDI_TransportMnemonic, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((mnemonic.length() + 1) * sizeof(WCHAR)), (PVOID)mnemonic.c_str() });

        interfaceDevProperties.push_back({ {PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

        interfaceDevProperties.push_back({ {PKEY_MIDI_GenerateIncomingTimestamp, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

        // Adding this here so it can later be updated in-protocol.
        MidiDeviceIdentityProperty dummyDeviceIdentity;
        interfaceDevProperties.push_back({ {PKEY_MIDI_DeviceIdentity, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(dummyDeviceIdentity)), &dummyDeviceIdentity });

        // default to keep us from spamming JR timestamps until they are configured
        interfaceDevProperties.push_back({ {PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), &devPropFalse });
        interfaceDevProperties.push_back({ {PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), &devPropFalse });



        // TODO: iSerialNumber from driver KS property PKEY_MIDI_SerialNumber

        // TODO: Manufacturer name from driver KS property PKEY_MIDI_ManufacturerName



        if (MidiPin->NativeDataFormat != GUID_NULL)
        {
            BYTE nativeDataFormat {0};

            if (MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
            {
                nativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
            }
            else if (MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_MIDI)
            {
                nativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM;
            }
            else
            {
                RETURN_IF_FAILED(E_UNEXPECTED);
            }

            interfaceDevProperties.push_back({ { PKEY_MIDI_NativeDataFormat, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(BYTE)), (PVOID) &nativeDataFormat });
        }

        if (MidiPin->GroupTerminalBlockDataSizeOut > 0)
        {
            interfaceDevProperties.push_back({ { PKEY_MIDI_OUT_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_BINARY, MidiPin->GroupTerminalBlockDataSizeOut, (PVOID)MidiPin->GroupTerminalBlockDataOut.get() });
        }

        if (MidiPin->GroupTerminalBlockDataSizeIn > 0)
        {
            interfaceDevProperties.push_back({ { PKEY_MIDI_IN_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_BINARY, MidiPin->GroupTerminalBlockDataSizeIn, (PVOID)MidiPin->GroupTerminalBlockDataIn.get() });
        }

        // Bidirectional uses a different property for the in and out pins, since we currently require two separate ones.
        if (MidiPin->Flow == MidiFlowBidirectional)
        {
            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_OutPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) });
            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_InPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinIdIn) });
        }
        else
        {
            // In and out have only a single pin id to create.
            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_KsPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) });
        }

        deviceDevProperties.push_back({ { DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

        SW_DEVICE_CREATE_INFO createInfo{ 0 };

        createInfo.cbSize = sizeof(createInfo);
        createInfo.pszInstanceId = MidiPin->InstanceId.c_str();
        createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
        createInfo.pszDeviceDescription = MidiPin->Name.c_str();

        const ULONG deviceInterfaceIdMaxSize = 255;
        wchar_t newDeviceInterfaceId[deviceInterfaceIdMaxSize]{ 0 };

        // log telemetry in the event activating the SWD for this pin has failed,
        // but push forward with creation for other pins.
        LOG_IF_FAILED(MidiPin->SwdCreation = m_MidiDeviceManager->ActivateEndpoint(
                                                            MidiPin->ParentInstanceId.c_str(),
                                                            MidiPin->CreateUMPOnly,
                                                            MidiPin->Flow,
                                                            (ULONG) interfaceDevProperties.size(),
                                                            (ULONG) deviceDevProperties.size(),
                                                            (PVOID)interfaceDevProperties.data(),
                                                            (PVOID)deviceDevProperties.data(),
                                                            (PVOID)&createInfo,
                                                            (LPWSTR)&newDeviceInterfaceId,
                                                            deviceInterfaceIdMaxSize));

        
        // Now we deal with metadata provided in-protocol and also by the user

        if (SUCCEEDED(MidiPin->SwdCreation))
        {
            // clear any of the endpoint-discovered properties that hung around from the last time
            // we saw this device, if we've ever seen it.
            m_MidiDeviceManager->DeleteAllEndpointInProtocolDiscoveredProperties(newDeviceInterfaceId);

            // load settings from the configuration JSON and update properties
            LOG_IF_FAILED(ApplyUserConfiguration(std::wstring(newDeviceInterfaceId)));


            // we only perform protocol negotiation if it's a bidirectional UMP (native) endpoint. We
            // don't want to perform this on translated byte stream endpoints
            if (MidiPin->Flow == MidiFlowBidirectional && MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
            {
                // Required MIDI 2.0 Protocol step
                // Invoke the protocol negotiator to now capture updated endpoint info.

                bool preferToSendJRToEndpoint{ false };
                bool preferToReceiveJRFromEndpoint{ false };
                BYTE preferredProtocol{ MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2 };
                WORD negotiationTimeoutMS{ 5000 };  // this should be much shorter

                LOG_IF_FAILED(m_MidiProtocolManager->NegotiateAndRequestMetadata(
                    newDeviceInterfaceId,
                    preferToSendJRToEndpoint,
                    preferToReceiveJRFromEndpoint,
                    preferredProtocol,
                    negotiationTimeoutMS
                ));
            }

        }

    }

    // move the newMidiPins to the m_AvailableMidiPins list, which contains a list of
    // all KS pins (and their swd's), which exist on the system.
    while (newMidiPins.size() > 0)
    {
        m_AvailableMidiPins.push_back(std::move(newMidiPins.back()));
        newMidiPins.pop_back();
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2KSMidiEndpointManager::ApplyUserConfiguration(std::wstring deviceInterfaceId)
{
    OutputDebugString(L"\n" __FUNCTION__);

    try
    {
        // if we have no configuration, that's ok
        if (m_jsonObject == nullptr) return S_OK;


        std::vector<DEVPROPERTY> endpointProperties;

        // for now, we only support lookup by the deviceInterfaceId, so this code is easier

        winrt::hstring endpointSettingsKey = winrt::to_hstring(MIDI_CONFIG_JSON_ENDPOINT_IDENTIFIER_SWD) + deviceInterfaceId;

        //OutputDebugString(L" Key: ");
        //OutputDebugString(endpointSettingsKey.c_str());
        //OutputDebugString(L"\n");


        if (m_jsonObject.HasKey(endpointSettingsKey))
        {
            json::JsonObject endpointSettings = m_jsonObject.GetNamedObject(endpointSettingsKey);

            // Get the user-specified endpoint name
            if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY))
            {
                auto name = endpointSettings.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_NAME_PROPERTY_KEY);

                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_STRING, static_cast<ULONG>((name.size() + 1) * sizeof(WCHAR)), (PVOID)name.c_str() });
            }
            else
            {
                // delete any existing property value, because it is no longer in the config
                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedEndpointName, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_EMPTY, 0, nullptr });
            }


            // Get the user-specified endpoint description
            if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY))
            {
                auto description = endpointSettings.GetNamedString(MIDI_CONFIG_JSON_ENDPOINT_USER_SUPPLIED_DESCRIPTION_PROPERTY_KEY);

                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_STRING, static_cast<ULONG>((description.size() + 1) * sizeof(WCHAR)), (PVOID)description.c_str() });
            }
            else
            {
                // delete any existing property value, because it is no longer in the config
                endpointProperties.push_back({ {PKEY_MIDI_UserSuppliedDescription, DEVPROP_STORE_SYSTEM, nullptr},
                        DEVPROP_TYPE_EMPTY, 0, nullptr });
            }

            // Get the user-specified multiclient override
            if (endpointSettings != nullptr && endpointSettings.HasKey(MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY))
            {
                auto forceSingleClient = endpointSettings.GetNamedBoolean(MIDI_CONFIG_JSON_ENDPOINT_FORCE_SINGLE_CLIENT_PROPERTY_KEY);

                if (forceSingleClient)
                {
                    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

                    endpointProperties.push_back({ {PKEY_MIDI_SupportsMulticlient, DEVPROP_STORE_SYSTEM, nullptr},
                            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropFalse)), (PVOID)&devPropFalse });
                }
            }
            else
            {
                // this property was an override, so it should have been set elsewhere earlier
            }

            // apply supported property changes.
            if (endpointProperties.size() > 0)
            {
                return m_MidiDeviceManager->UpdateEndpointProperties(
                    (LPCWSTR)deviceInterfaceId.c_str(),
                    (ULONG)endpointProperties.size(),
                    (PVOID)endpointProperties.data());
            }
        }

        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }
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
