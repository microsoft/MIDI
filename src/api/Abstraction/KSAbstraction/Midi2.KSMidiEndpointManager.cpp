// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


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
    IUnknown* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManagerInterface), (void**)&m_MidiProtocolManager));

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
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(device.Id().c_str(), "added device")
    );


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

        std::unique_ptr<WCHAR> serialNumberData;
        ULONG serialNumberDataSize{ 0 };
        std::unique_ptr<WCHAR> manufacturerNameData;
        ULONG manufacturerNameDataSize{ 0 };

        UINT16 deviceVID{ 0 };
        UINT16 devicePID{ 0 };

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
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "added device"),
                TraceLoggingWideString(L"Pin is a Cyclic UMP MIDI pin", "message")
            );

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

            //// Get the serial number
            //LOG_IF_FAILED_WITH_EXPECTED(PinPropertyAllocate(hPin.get(),
            //                                i,
            //                                KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //                                KSPROPERTY_MIDI2_SERIAL_NUMBER,
            //                                (PVOID*)&serialNumberData,
            //                                &serialNumberDataSize),
            //                                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));


            //// Get the manufacturer name
            //LOG_IF_FAILED_WITH_EXPECTED(PinPropertyAllocate(hPin.get(),
            //                                i,
            //                                KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //                                KSPROPERTY_MIDI2_DEVICE_MANUFACTURER,
            //                                (PVOID*)&manufacturerNameData,
            //                                &manufacturerNameDataSize),
            //                                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            //// VID iVendor
            //LOG_IF_FAILED_WITH_EXPECTED(PinPropertySimple(hPin.get(),
            //                                i,
            //                                KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //                                KSPROPERTY_MIDI2_DEVICE_VID,
            //                                &deviceVID,
            //                                sizeof(deviceVID)),
            //                                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            //// PID iProduct
            //LOG_IF_FAILED_WITH_EXPECTED(PinPropertySimple(hPin.get(),
            //                                i,
            //                                KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //                                KSPROPERTY_MIDI2_DEVICE_PID,
            //                                &devicePID,
            //                                sizeof(devicePID)),
            //                                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

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

        midiPin->VID = deviceVID;
        midiPin->PID = devicePID;

        // Group Terminal Blocks from the driver

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

        // Native Data format (UMP or bytestream) from the driver

        midiPin->NativeDataFormat = nativeDataFormat;


        // Serial number from the driver. Empty serials are a 1 character nul, so check for size > sizeof(WCHAR)

        if (serialNumberDataSize > sizeof(WCHAR))
        {
            midiPin->SerialNumber = std::wstring(serialNumberData.get(), (size_t)(serialNumberDataSize / sizeof(WCHAR)));

            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(midiPin->SerialNumber.c_str(), "serial number"),
                TraceLoggingULong(serialNumberDataSize, "data size")
            );
        }
        else
        {
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(L"No serial number", "serial number")
            );
        }

        // Manufacturer from the driver. Empty manufacturer is a 1 character nul, so check for size > sizeof(WCHAR)

        if (manufacturerNameDataSize > sizeof(WCHAR))
        {
            midiPin->ManufacturerName = std::wstring(manufacturerNameData.get(), (size_t)(manufacturerNameDataSize / sizeof(WCHAR)));

            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(midiPin->ManufacturerName.c_str(), "manufacturer"),
                TraceLoggingULong(manufacturerNameDataSize, "data size")
            );
        }
        else
        {
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(L"No manufacturer name", "manufacturer")
            );
        }

        midiPin->InstanceId = L"MIDIU_KS_";
        midiPin->InstanceId += (midiPin->Flow == MidiFlowOut) ? L"OUT_" : L"IN_";
        midiPin->InstanceId += hash;
        midiPin->InstanceId += L"_PIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinId);

        newMidiPins.push_back(std::move(midiPin));
    }

#ifdef CREATE_KS_BIDI_SWDS
    // TODO: This logic needs to change because we want ALL KS devices to show up
    // as bidi, even if they have only a single pin. We need to map each to a logical
    // group instead.
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

        midiPin->SerialNumber = newMidiPins[midiInPinIndex]->SerialNumber;
        midiPin->ManufacturerName = newMidiPins[midiInPinIndex]->ManufacturerName;

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


        // TODO: If provided, hash using the serial?

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
        //DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
        //DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

        std::vector<DEVPROPERTY> interfaceDevProperties;
        //std::vector<DEVPROPERTY> deviceDevProperties;


        MIDIENDPOINTCOMMONPROPERTIES commonProperties {};
        commonProperties.AbstractionLayerGuid = KsAbstractionLayerGUID;
        commonProperties.EndpointPurpose = MidiEndpointDevicePurposePropertyValue::NormalMessageEndpoint;
        commonProperties.FriendlyName = MidiPin->Name.c_str();
        commonProperties.TransportMnemonic = mnemonic.c_str();
        commonProperties.TransportSuppliedEndpointName = MidiPin->Name.c_str();
        commonProperties.TransportSuppliedEndpointDescription = nullptr;
        commonProperties.UserSuppliedEndpointName = nullptr;
        commonProperties.UserSuppliedEndpointDescription = nullptr;
        commonProperties.UniqueIdentifier = MidiPin->SerialNumber.c_str();
        commonProperties.ManufacturerName = MidiPin->ManufacturerName.c_str();
        commonProperties.SupportedDataFormats = MidiPin->DataFormatCapability;
        commonProperties.SupportsMultiClient = true;
        commonProperties.GenerateIncomingTimestamps = true;


        interfaceDevProperties.push_back({ {DEVPKEY_KsMidiPort_KsFilterInterfaceId, DEVPROP_STORE_SYSTEM, nullptr},
                DEVPROP_TYPE_STRING, static_cast<ULONG>((MidiPin->Id.length() + 1) * sizeof(WCHAR)), (PVOID)MidiPin->Id.c_str() });
        interfaceDevProperties.push_back({ {DEVPKEY_KsTransport, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), (PVOID)&MidiPin->TransportCapability });

        // VID and PID

        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbVID, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT16, static_cast<ULONG>(sizeof(UINT16)), (PVOID)&(MidiPin->VID) });

        interfaceDevProperties.push_back({ { PKEY_MIDI_UsbPID, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT16, static_cast<ULONG>(sizeof(UINT16)), (PVOID)&(MidiPin->PID) });

        if (MidiPin->NativeDataFormat != GUID_NULL)
        {
            if (MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
            {
                // default native UMP devices to support MIDI 1.0 and MIDI 2.0 until they negotiation otherwise
                // These properties were originally meant only for the results of protocol negotiation, but they were confusing when not set

                commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
                commonProperties.RequiresMetadataHandler = true;
                commonProperties.SupportsMidi1ProtocolDefaultValue = true;
                commonProperties.SupportsMidi2ProtocolDefaultValue = true;
            }
            else if (MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_MIDI)
            {
                // default bytestream devices to MIDI 1.0 protocol only
                // These properties were originally meant only for the results of protocol negotiation, but they were confusing when not set

                commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_BYTESTREAM;
                commonProperties.RequiresMetadataHandler = false;
                commonProperties.SupportsMidi1ProtocolDefaultValue = true;
                commonProperties.SupportsMidi2ProtocolDefaultValue = false;
            }
            else
            {
                RETURN_IF_FAILED(E_UNEXPECTED);
            }
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

        //deviceDevProperties.push_back({ { DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr },
        //        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

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
                                                            &commonProperties,
                                                            (ULONG) interfaceDevProperties.size(),
                                                            (ULONG)0,
                                                            (PVOID)interfaceDevProperties.data(),
                                                            (PVOID)nullptr,
                                                            (PVOID)&createInfo,
                                                            (LPWSTR)&newDeviceInterfaceId,
                                                            deviceInterfaceIdMaxSize));

        
        // Now we deal with metadata provided in-protocol and also by the user

        if (SUCCEEDED(MidiPin->SwdCreation))
        {
            // Here we're only building search keys for the SWD id, but we need to broaden this to
            // other relevant search criteria like serial number or the original name, etc.

            auto jsonSearchKeys = AbstractionState::Current().GetConfigurationManager()->BuildEndpointJsonSearchKeysForSWD(newDeviceInterfaceId);

            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(jsonSearchKeys.c_str(), "jsonSearchKeys")
            );

            auto applyConfigHR = AbstractionState::Current().GetConfigurationManager()->ApplyConfigFileUpdatesForEndpoint(jsonSearchKeys);

            if (FAILED(applyConfigHR))
            {
                TraceLoggingWrite(
                    MidiKSAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(jsonSearchKeys.c_str(), "jsonSearchKeys"),
                    TraceLoggingHResult(applyConfigHR, "hresult"),
                    TraceLoggingWideString(L"Unable to apply config file update for endpoint", "message")
                );

                LOG_IF_FAILED(applyConfigHR);
            }

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

                PENDPOINTPROTOCOLNEGOTIATIONRESULTS negotiationResults;

                LOG_IF_FAILED(m_MidiProtocolManager->NegotiateAndRequestMetadata(
                    newDeviceInterfaceId,
                    preferToSendJRToEndpoint,
                    preferToReceiveJRFromEndpoint,
                    preferredProtocol,
                    negotiationTimeoutMS,
                    &negotiationResults
                ));


                // TODO: The negotiationResults structure contains the function blocks which 
                // should be used to create MIDI 1.0 legacy ports for this MIDI 2.0 device.


            }

        }
        else
        {
            TraceLoggingWrite(
                MidiKSAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingHResult(MidiPin->SwdCreation, "hresult"),
                TraceLoggingWideString(L"Failed to activate endpoint", "message")
            );
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
HRESULT CMidi2KSMidiEndpointManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate device)
{
    TraceLoggingWrite(
        MidiKSAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(device.Id().c_str(), "device id")
    );

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
