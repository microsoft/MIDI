// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "midi2.kstransport.h"

using namespace wil;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

// if this is defined, then KSMidiEndpointManager will publish a Bidi endpoint
// for pairs of midi in & out endpoints on the same filter.

#define CREATE_KS_BIDI_SWDS
#define BIDI_REPLACES_INOUT_SWDS

#define INITIAL_ENUMERATION_TIMEOUT_MS 10000

_Use_decl_annotations_
HRESULT
CMidi2KSMidiEndpointManager::Initialize(
    IMidiDeviceManager* midiDeviceManager,
    IMidiEndpointProtocolManager* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );

    RETURN_HR_IF(E_INVALIDARG, nullptr == midiDeviceManager);
    RETURN_HR_IF(E_INVALIDARG, nullptr == midiEndpointProtocolManager);

    RETURN_IF_FAILED(midiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManager), (void**)&m_midiDeviceManager));
    RETURN_IF_FAILED(midiEndpointProtocolManager->QueryInterface(__uuidof(IMidiEndpointProtocolManager), (void**)&m_midiProtocolManager));

    winrt::hstring deviceSelector(
        L"System.Devices.InterfaceClassGuid:=\"{6994AD04-93EF-11D0-A3CC-00A0C9223196}\" AND " \
        L"System.Devices.InterfaceEnabled: = System.StructuredQueryType.Boolean#True");

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

    // Wait for everything to be created so that they're available immediately after service start.
    m_EnumerationCompleted.wait(INITIAL_ENUMERATION_TIMEOUT_MS);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2KSMidiEndpointManager::OnDeviceAdded(
    DeviceWatcher , 
    DeviceInformation device
)
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"OnDeviceAdded. Processing new device.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device.Id().c_str(), "device id")
    );

//    DEVPROP_BOOLEAN devPropTrue = DEVPROP_TRUE;
//    DEVPROP_BOOLEAN devPropFalse = DEVPROP_FALSE;

    std::wstring deviceName;
    std::wstring filterName{ device.Name() };
    std::wstring deviceId;
    std::wstring deviceInstanceId;
    std::hash<std::wstring> hasher;
    std::wstring hash;
    ULONG cPins{ 0 };

    std::wstring transportCode(TRANSPORT_CODE);

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    auto properties = device.Properties();

    // retrieve the device instance id from the DeviceInformation property store
    auto prop = properties.Lookup(L"System.Devices.DeviceInstanceId");
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
    //
    // MIDI 1 port Name hierarchy for friendly name (and generated GTB name) from most preferred to least
    // - User-supplied Name from config
    // - iJack (some devices use this for the port name)
    // - iInterface (+ index if more than one pin)
    // - iProduct from USB (+ index if more than one pin)
    //
    // A single device can have multiple filters. Roland / Yamaha would prefer we aggregate those
    // when creating the bidi interface.
    //
    // The SWD id generated for a MIDI 2.0 UMP endpoint should have the iSerialNumber, if provided, as the
    // primary differentiator in the id. No hashing required unless it is too long.
    //

    auto parentDeviceInfo = DeviceInformation::CreateFromIdAsync(deviceInstanceId,
        additionalProperties,winrt::Windows::Devices::Enumeration::DeviceInformationKind::Device).get();
    deviceName = parentDeviceInfo.Name();

    hash = std::to_wstring(hasher(deviceId));

    std::vector<std::unique_ptr<MIDI_PIN_INFO>> newMidiPins;

    // instantiate the interface

    // Wrapper opens the handle internally.
    KsHandleWrapper deviceHandleWrapper(deviceId.c_str());
    RETURN_IF_FAILED(deviceHandleWrapper.Open());

    // Using lamba function to prevent handle from dissapearing when being used. 
    RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
        return PinPropertySimple(h, 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins));
    }));

    //UINT portNumberDifferentiatorInput{ 0 };
    //UINT portNumberDifferentiatorOutput{ 0 };

    for (UINT i = 0; i < cPins; i++)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Processing pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device.Id().c_str(), "device id"),
            TraceLoggingUInt32(i, "pin id")
        );

        KSPIN_DATAFLOW dataFlow = (KSPIN_DATAFLOW)0;
        MidiTransport transportCapability { MidiTransport_Invalid };
        MidiDataFormats dataFormatCapability { MidiDataFormats_Invalid };
        KSPIN_COMMUNICATION communication = (KSPIN_COMMUNICATION)0;
        GUID nativeDataFormat {0};

        std::unique_ptr<BYTE> groupTerminalBlockData;
        ULONG groupTerminalBlockDataSize {0};

        std::unique_ptr<WCHAR> serialNumberData;
        ULONG serialNumberDataSize{ 0 };

        std::unique_ptr<WCHAR> manufacturerNameData;
        ULONG manufacturerNameDataSize{ 0 };

        //std::unique_ptr<WCHAR> pinNameData;
        //ULONG pinNameDataSize{ 0 };

        UINT16 deviceVID{ 0 };
        UINT16 devicePID{ 0 };

        RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
            return PinPropertySimple(h, i, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION));
        }));

        // The external connector pin representing the physical connection
        // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
        // have a valid communication. Skip connector pins.
        if (KSPIN_COMMUNICATION_NONE == communication)
        {
            //TraceLoggingWrite(
            //    MidiKSTransportTelemetryProvider::Provider(),
            //    MIDI_TRACE_EVENT_INFO,
            //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            //    TraceLoggingPointer(this, "this"),
            //    TraceLoggingWideString(L"Pin is not for communication. Moving on", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            //    TraceLoggingWideString(device.Id().c_str(), "device id"),
            //    TraceLoggingUInt32(i, "pin id")
            //);

            continue;
        }

        // ================== Cyclic UMP Interfaces ===============================================
        // attempt to instantiate using cyclic streaming for UMP buffers
        // and set that flag if we can.

        // Duplicate the handle to safely pass it to another component or store it.
        wil::unique_handle handleDupe(deviceHandleWrapper.GetHandle());
        RETURN_IF_NULL_ALLOC(handleDupe);

        KsHandleWrapper m_PinHandleWrapper(deviceId.c_str(), i, MidiTransport_CyclicUMP, handleDupe.get());

        RETURN_IF_FAILED(m_PinHandleWrapper.Open());

        // Using lamba function to prevent handle from dissapearing when being used.
        RETURN_IF_FAILED(m_PinHandleWrapper.Execute([&](HANDLE handle) {

            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Pin is MidiTransport_CyclicUMP pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingUInt32(i, "pin id"));

            transportCapability = (MidiTransport)((DWORD)transportCapability | (DWORD)MidiTransport_CyclicUMP);
            dataFormatCapability = (MidiDataFormats)((DWORD)dataFormatCapability | (DWORD)MidiDataFormats_UMP);

            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertySimple(
                    handle,
                    i,
                    KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
                    KSPROPERTY_MIDI2_NATIVEDATAFORMAT,
                    &nativeDataFormat,
                    sizeof(nativeDataFormat)),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertyAllocate(
                    handle,
                    i,
                    KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
                    KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,
                    (PVOID*)&groupTerminalBlockData,
                    &groupTerminalBlockDataSize),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            // Get the serial number
            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertyAllocate(
                    handle, i, KSPROPSETID_MIDI2_ENDPOINT_INFORMATION, KSPROPERTY_MIDI2_SERIAL_NUMBER, (PVOID*)&serialNumberData, &serialNumberDataSize),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            // Get the manufacturer name
            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertyAllocate(
                    handle,
                    i,
                    KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
                    KSPROPERTY_MIDI2_DEVICE_MANUFACTURER,
                    (PVOID*)&manufacturerNameData,
                    &manufacturerNameDataSize),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            // VID iVendor
            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertySimple(
                    handle, i, KSPROPSETID_MIDI2_ENDPOINT_INFORMATION, KSPROPERTY_MIDI2_DEVICE_VID, &deviceVID, sizeof(deviceVID)),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            // PID iProduct
            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertySimple(
                    handle, i, KSPROPSETID_MIDI2_ENDPOINT_INFORMATION, KSPROPERTY_MIDI2_DEVICE_PID, &devicePID, sizeof(devicePID)),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            return S_OK;
        }));

        // if this pin supports nothing, then it's not a streaming pin,
        // continue on
        if (0 == transportCapability)
        {
            //TraceLoggingWrite(
            //    MidiKSTransportTelemetryProvider::Provider(),
            //    MIDI_TRACE_EVENT_INFO,
            //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            //    TraceLoggingPointer(this, "this"),
            //    TraceLoggingWideString(L"Pin has no transport capability", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            //    TraceLoggingWideString(device.Id().c_str(), "device id"),
            //    TraceLoggingUInt32(i, "pin id")
            //);

            continue;
        }

        RETURN_IF_FAILED(deviceHandleWrapper.Execute([&](HANDLE h) -> HRESULT {
            return PinPropertySimple(h, i, KSPROPSETID_Pin, KSPROPERTY_PIN_DATAFLOW, &dataFlow, sizeof(KSPIN_DATAFLOW));
        }));

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

        // Group Terminal Blocks from the MIDI 2 driver
        midiPin->GroupTerminalBlockData = std::move(groupTerminalBlockData);
        midiPin->GroupTerminalBlockDataSize = groupTerminalBlockDataSize;

        // Native Data format (UMP or bytestream) from the driver

        midiPin->NativeDataFormat = nativeDataFormat;


        // Serial number from the driver. Empty serials are a 1 character nul, so check for size > sizeof(WCHAR)

        if (serialNumberDataSize > sizeof(WCHAR))
        {
            midiPin->SerialNumber = std::wstring(serialNumberData.get(), (size_t)(serialNumberDataSize / sizeof(WCHAR)));

            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Retrieved serial number", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(midiPin->SerialNumber.c_str(), "serial number"),
                TraceLoggingULong(serialNumberDataSize, "data size")
            );
        }
        else
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No USB device serial number", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(device.Id().c_str(), "device id")
            );
        }

        // Manufacturer from the driver. Empty manufacturer is a 1 character nul, so check for size > sizeof(WCHAR)

        if (manufacturerNameDataSize > sizeof(WCHAR))
        {
            midiPin->ManufacturerName = std::wstring(manufacturerNameData.get(), (size_t)(manufacturerNameDataSize / sizeof(WCHAR)));

            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Retrieved manufacturer name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(midiPin->ManufacturerName.c_str(), "manufacturer"),
                TraceLoggingULong(manufacturerNameDataSize, "data size")
            );
        }
        else
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"No USB device manufacturer name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(device.Id().c_str(), "device id")
            );
        }

        midiPin->InstanceId = MIDI_KS_INSTANCE_ID_PREFIX;
        midiPin->InstanceId += (midiPin->Flow == MidiFlowOut) ? L"OUT_" : L"IN_";

        // serial# alone is not enough because it isn't unique. Need to also have VID and PID
        // but this is a problem when a serial number is reused by the manufacturer, which
        // happens all too often. So we'll just use the hash of the parent device instance id
        // and let Windows create the unique id. Need to test to see if iSerialNumber really
        // is working in this case.
        // 
        // TODO: We may want to actually check and see if there are others already with 
        // this id, and simply add an index to the generated instance id. We need to 
        // optimize for well-behaved devices and just not break for others.

        //if (midiPin->VID > 0 && midiPin->PID > 0 && !midiPin->SerialNumber.empty())
        //{
        //    midiPin->InstanceId +=
        //        L"V_" + std::to_wstring(midiPin->VID) +
        //        L"P_" + std::to_wstring(midiPin->PID) +
        //        L"S_" + midiPin->SerialNumber;
        //}
        //else
        //{
            midiPin->InstanceId += hash;
        // }

        midiPin->InstanceId += L"_PIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinId);

        newMidiPins.push_back(std::move(midiPin));
    }

#ifdef CREATE_KS_BIDI_SWDS

    // UMP devices only. Some redundant checking here, but just in case
    if (newMidiPins.size() == 2 && 
        (newMidiPins[0]->Flow != newMidiPins[1]->Flow) &&
        (newMidiPins[0]->TransportCapability == newMidiPins[1]->TransportCapability) &&
        (newMidiPins[0]->DataFormatCapability & MidiDataFormats::MidiDataFormats_UMP) == MidiDataFormats::MidiDataFormats_UMP )
    {
        //TraceLoggingWrite(
        //    MidiKSTransportTelemetryProvider::Provider(),
        //    MIDI_TRACE_EVENT_INFO,
        //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        //    TraceLoggingPointer(this, "this"),
        //    TraceLoggingWideString(L"Setting up to create UMP bidirectional endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        //    TraceLoggingWideString(device.Id().c_str(), "device id")
        //);

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

        // in and out pins should have the same GTB's, pick one.
        if (newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSize > 0)
        {
            std::unique_ptr<BYTE> groupTerminalBlockData(new (std::nothrow) BYTE[newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSize]);
            RETURN_IF_NULL_ALLOC(groupTerminalBlockData);
            memcpy(groupTerminalBlockData.get(), newMidiPins[midiOutPinIndex]->GroupTerminalBlockData.get(), newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSize);
            midiPin->GroupTerminalBlockData = std::move(groupTerminalBlockData);
            midiPin->GroupTerminalBlockDataSize = newMidiPins[midiOutPinIndex]->GroupTerminalBlockDataSize;
        }

        midiPin->NativeDataFormat = newMidiPins[midiOutPinIndex]->NativeDataFormat;
        midiPin->Flow = MidiFlowBidirectional;

        midiPin->InstanceId = MIDI_KS_INSTANCE_ID_PREFIX;

        //if (midiPin->SerialNumber.empty())
        //{
            midiPin->InstanceId += hash;
        //}
        //else
        //{
        //    midiPin->InstanceId += midiPin->SerialNumber;
        //}

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

        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"New pin added", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(device.Id().c_str(), "device id")
        );
    }
    
#endif

    //in the added callback we're going to go ahead and create the SWD's:
    for (auto const& MidiPin : newMidiPins)
    {
        TraceLoggingWrite(
            MidiKSTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"creating endpoint from pin", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(MidiPin->Name.c_str(), "pin name"),
            TraceLoggingUInt32(MidiPin->PinId, "pin id")
        );

        GUID KsTransportLayerGUID = TRANSPORT_LAYER_GUID;

        std::vector<DEVPROPERTY> interfaceDevProperties;

        MIDIENDPOINTCOMMONPROPERTIES commonProperties {};
        commonProperties.TransportId = KsTransportLayerGUID;
        commonProperties.EndpointDeviceType = MidiEndpointDeviceType_Normal;
        commonProperties.FriendlyName = MidiPin->Name.c_str();
        commonProperties.TransportCode = transportCode.c_str();
        commonProperties.EndpointName = MidiPin->Name.c_str();
        commonProperties.EndpointDescription = nullptr;
        commonProperties.CustomEndpointName = nullptr;
        commonProperties.CustomEndpointDescription = nullptr;
        commonProperties.UniqueIdentifier = MidiPin->SerialNumber.c_str();
        commonProperties.ManufacturerName = MidiPin->ManufacturerName.c_str();
        commonProperties.SupportedDataFormats = MidiPin->DataFormatCapability;

        UINT32 capabilities {0};
        capabilities |= MidiEndpointCapabilities_SupportsMultiClient;
        capabilities |= MidiEndpointCapabilities_GenerateIncomingTimestamps;


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
                TraceLoggingWrite(
                    MidiKSTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin native data format is UMP", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(MidiPin->Name.c_str(), "pin name")
                );

                // default native UMP devices to support MIDI 1.0 and MIDI 2.0 until they negotiation otherwise
                // These properties were originally meant only for the results of protocol negotiation, but they were confusing when not set

                commonProperties.NativeDataFormat = MidiDataFormats_UMP;
                capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;
                capabilities |= MidiEndpointCapabilities_SupportsMidi2Protocol;

                // for native MIDI 2 devices, we use filter name + pin (gtb) name
                // TODO: This needs to be re-done to use user-specified properties from config file
                auto naming = Midi1PortNameSelectionProperty::PortName_UseFilterPlusBlockName;
                interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_UINT32, (ULONG)sizeof(Midi1PortNameSelectionProperty), (PVOID)&naming });
            }
            else if (MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_MIDI)
            {
                TraceLoggingWrite(
                    MidiKSTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin native data format is bytestream", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(MidiPin->Name.c_str(), "pin name")
                );

                // default bytestream devices to MIDI 1.0 protocol only
                // These properties were originally meant only for the results of protocol negotiation, but they were confusing when not set

                commonProperties.NativeDataFormat = MidiDataFormats_ByteStream;
                capabilities |= MidiEndpointCapabilities_SupportsMidi1Protocol;

                //
                // TODO: This needs to be updated to use user-specified properties from config file
                //
                auto naming = Midi1PortNameSelectionProperty::PortName_UseGlobalDefault;
                interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_UINT32, (ULONG)sizeof(Midi1PortNameSelectionProperty), (PVOID)&naming });
            }
            else
            {
                RETURN_IF_FAILED(E_UNEXPECTED);
            }
        }

        commonProperties.Capabilities = (MidiEndpointCapabilities) capabilities;

        if (MidiPin->GroupTerminalBlockDataSize > 0)
        {
            interfaceDevProperties.push_back({ { PKEY_MIDI_GroupTerminalBlocks, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_BINARY, MidiPin->GroupTerminalBlockDataSize, (PVOID)MidiPin->GroupTerminalBlockData.get() });
        }


        // Bidirectional uses a different property for the in and out pins, since we currently require two separate ones.
        if (MidiPin->Flow == MidiFlowBidirectional)
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Pin is MidiFlowBidirectional", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(MidiPin->Name.c_str(), "pin name")
            );

            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_OutPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) });

            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_InPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinIdIn) });
        }
        else if (MidiPin->Flow == MidiFlowIn || MidiPin->Flow == MidiFlowOut)
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Pin is unidirectional", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(MidiPin->Name.c_str(), "pin name")
            );

            // In and out have only a single pin id to create.
            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_KsPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) });
        }


        // Write Name table property
        // =====================================================

        // we have group terminal blocks data, so we need to create names starting from that

        // these variables need to be out here, to make sure the data is still available when the properties are created
        std::vector<std::byte> nameTablePropertyData{ };
        std::vector<internal::Midi1PortNaming::Midi1PortNameEntry> portNameEntries{};
        //
        // TODO: This needs to be re-done to use user-specified properties from config file
        //
        auto naming = Midi1PortNameSelectionProperty::PortName_UseGlobalDefault;


        if (!MidiPin->CreateUMPOnly)
        {
            // get the group terminal blocks and their names, because that's how the MIDI 2 driver reports the info about attached MIDI 1 devices
            // for MIDI 2.0 USB devices, we start with the GTBs, but after discovery and protocol negotiation, will update them from function blocks
            auto gtbs = internal::ReadGroupTerminalBlocksFromPropertyData(MidiPin->GroupTerminalBlockData.get(), MidiPin->GroupTerminalBlockDataSize);

            for (auto const& gtb : gtbs)
            {
                // process each group. For MIDI 1 devices, count will always be 1. For MIDI 2, it could be anything from 1-16
                for (uint8_t groupIndex = gtb.FirstGroupIndex; groupIndex < gtb.FirstGroupIndex + gtb.GroupCount; groupIndex++)
                {
                    std::wstring nameFromRegistry = L"";    // we don't have this with MIDI 2 devices
                    std::wstring pinName = gtb.Name;        // for MIDI 1.0 devices connected using the new driver, this is the name to use

                    // TODO: Here's where we should load any custom port name into the table, using the values from config
                    std::wstring customPortName = L"";      // TODO: Get from config

                    if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                        gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_OUTPUT)              // gtb output is a midi input
                    {
                        // we need to create two entries for bidi
                        internal::Midi1PortNaming::Midi1PortNameEntry nameEntry{ };

                        nameEntry.GroupIndex = groupIndex;
                        nameEntry.DataFlowFromUserPerspective = MidiFlow::MidiFlowIn;

                        // MIDI Input
                        internal::Midi1PortNaming::PopulateMidi1PortNameEntryNames(
                            nameEntry,
                            nameFromRegistry,
                            deviceName,                 // parent device name
                            filterName,                 // filter name. For MIDI 2 devices, this ends up the same as the parent device name
                            pinName,                    // we use gtb name here because the driver populates that with the pin name
                            customPortName,
                            MidiFlow::MidiFlowIn,
                            groupIndex,
                            groupIndex
                        );

                        portNameEntries.push_back(nameEntry);

                        TraceLoggingWrite(
                            MidiKSTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Added midi input port name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingUInt16(groupIndex, "group index"),
                            TraceLoggingWideString(gtb.Name.c_str(), "gtb name")
                        );
                    }

                    if (gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_BIDIRECTIONAL ||
                        gtb.Direction == MIDI_GROUP_TERMINAL_BLOCK_INPUT)
                    {
                        internal::Midi1PortNaming::Midi1PortNameEntry nameEntry{ };

                        nameEntry.GroupIndex = groupIndex;
                        nameEntry.DataFlowFromUserPerspective = MidiFlow::MidiFlowOut;

                        // MIDI Output
                        internal::Midi1PortNaming::PopulateMidi1PortNameEntryNames(
                            nameEntry,
                            nameFromRegistry,
                            deviceName,                 // parent device name
                            filterName,                 // filter name. For MIDI 2 devices, this ends up the same as the parent device name
                            pinName,                    // we use gtb name here because the driver populates that with the pin name for MIDI 1 devices
                            customPortName,
                            MidiFlow::MidiFlowOut,
                            groupIndex,
                            groupIndex
                        );

                        portNameEntries.push_back(nameEntry);

                        TraceLoggingWrite(
                            MidiKSTransportTelemetryProvider::Provider(),
                            MIDI_TRACE_EVENT_INFO,
                            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                            TraceLoggingPointer(this, "this"),
                            TraceLoggingWideString(L"Added midi output port name", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                            TraceLoggingUInt16(groupIndex, "group index"),
                            TraceLoggingWideString(gtb.Name.c_str(), "gtb name")
                        );
                    }
                }
            }

            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingUInt32(static_cast<uint32_t>(portNameEntries.size()), "port name entries count")
            );

            if (internal::Midi1PortNaming::WriteMidi1PortNameTableToPropertyDataPointer(portNameEntries, nameTablePropertyData))
            {
                interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_UINT32, (ULONG)sizeof(Midi1PortNameSelectionProperty), (PVOID)&naming });

                interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_BINARY, (ULONG)nameTablePropertyData.size(), (PVOID)nameTablePropertyData.data() });
            }
            else
            {
                // write empty data
                interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNamingSelection, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_EMPTY, 0, nullptr });

                interfaceDevProperties.push_back({ { PKEY_MIDI_Midi1PortNameTable, DEVPROP_STORE_SYSTEM, nullptr },
                    DEVPROP_TYPE_EMPTY, 0, nullptr });


                TraceLoggingWrite(
                    MidiKSTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to write MIDI 1 port name table to property data", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );
            }
        }


        // ==============================================



        //deviceDevProperties.push_back({ { DEVPKEY_Device_PresenceNotForDevice, DEVPROP_STORE_SYSTEM, nullptr },
        //        DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(devPropTrue)), &devPropTrue });

        SW_DEVICE_CREATE_INFO createInfo{ 0 };

        createInfo.cbSize = sizeof(createInfo);
        createInfo.pszInstanceId = MidiPin->InstanceId.c_str();
        createInfo.CapabilityFlags = SWDeviceCapabilitiesNone;
        createInfo.pszDeviceDescription = MidiPin->Name.c_str();

        wil::unique_cotaskmem_string newDeviceInterfaceId;


        // log telemetry in the event activating the SWD for this pin has failed,
        // but push forward with creation for other pins.
        LOG_IF_FAILED(MidiPin->SwdCreation = m_midiDeviceManager->ActivateEndpoint(
                                                            MidiPin->ParentInstanceId.c_str(),
                                                            MidiPin->CreateUMPOnly,
                                                            MidiPin->Flow,
                                                            &commonProperties,
                                                            (ULONG)interfaceDevProperties.size(),
                                                            (ULONG)0,
                                                            interfaceDevProperties.data(),
                                                            nullptr,
                                                            &createInfo,
                                                            &newDeviceInterfaceId));

        
        // Now we deal with metadata provided in-protocol and also by the user

        if (SUCCEEDED(MidiPin->SwdCreation))
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"SWD Creation succeeded", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(MidiPin->Name.c_str(), "name")
            );


            // Here we're only building search keys for the SWD id, but we need to broaden this to
            // other relevant search criteria like serial number or the original name, etc.

            auto jsonSearchKeys = TransportState::Current().GetConfigurationManager()->BuildEndpointJsonSearchKeysForSWD(newDeviceInterfaceId.get());

            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Retrieved json search keys", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(jsonSearchKeys.c_str(), "jsonSearchKeys")
            );

            auto applyConfigHR = TransportState::Current().GetConfigurationManager()->ApplyConfigFileUpdatesForEndpoint(jsonSearchKeys);

            if (FAILED(applyConfigHR))
            {
                TraceLoggingWrite(
                    MidiKSTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to apply config file update for endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(jsonSearchKeys.c_str(), "jsonSearchKeys"),
                    TraceLoggingHResult(applyConfigHR, MIDI_TRACE_EVENT_HRESULT_FIELD)
                );

                LOG_IF_FAILED(applyConfigHR);
            }

            // we only perform protocol negotiation if it's a bidirectional UMP (native) endpoint. We
            // don't want to perform this on translated byte stream endpoints
            if (MidiPin->Flow == MidiFlowBidirectional && MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_UNIVERSALMIDIPACKET)
            {
                if (m_midiProtocolManager != nullptr && m_midiProtocolManager->IsEnabled())
                {
                    TraceLoggingWrite(
                        MidiKSTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_INFO,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Starting up protocol negotiator for endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingWideString(newDeviceInterfaceId.get(), MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                        TraceLoggingWideString(MidiPin->Id.c_str(), "Pin id")
                    );

                    // Required MIDI 2.0 Protocol step
                    // Invoke the protocol negotiator to now capture updated endpoint info.

                    ENDPOINTPROTOCOLNEGOTIATIONPARAMS negotiationParams{ };

                    negotiationParams.PreferredMidiProtocol = MIDI_PROP_CONFIGURED_PROTOCOL_MIDI2;
                    negotiationParams.PreferToSendJitterReductionTimestampsToEndpoint = false;
                    negotiationParams.PreferToReceiveJitterReductionTimestampsFromEndpoint = false;

                    LOG_IF_FAILED(m_midiProtocolManager->DiscoverAndNegotiate(
                        __uuidof(Midi2KSTransport),
                        newDeviceInterfaceId.get(),
                        negotiationParams
                    ));
                }
            }
        }
        else
        {
            TraceLoggingWrite(
                MidiKSTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingHResult(MidiPin->SwdCreation, MIDI_TRACE_EVENT_HRESULT_FIELD),
                TraceLoggingWideString(L"Failed to activate endpoint", MIDI_TRACE_EVENT_MESSAGE_FIELD)
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

    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"KS Device processing complete", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device.Id().c_str(), "device id")
    );

    return S_OK;
}



_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate device)
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Device removed", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(device.Id().c_str(), "device id")
    );

    // the interface is no longer active, search through our m_AvailableMidiPins to identify
    // every entry with this filter interface id, and remove the SWD and remove the pin(s) from
    // the m_AvailableMidiPins list.
    do
    {
        auto item = std::find_if(m_AvailableMidiPins.begin(), m_AvailableMidiPins.end(), [&](const std::unique_ptr<MIDI_PIN_INFO>& Pin)
        {
            if (device.Id() == Pin->Id)
            {
                TraceLoggingWrite(
                    MidiKSTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Removing endpoint / pin entry", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(device.Id().c_str(), "device id")
                );

                return true;
            }

            return false;
        });

        if (item == m_AvailableMidiPins.end())
        {
            break;
        }

        // notify the device manager using the InstanceId for this midi device
        LOG_IF_FAILED(m_midiDeviceManager->RemoveEndpoint(item->get()->InstanceId.c_str()));

        // remove the MIDI_PIN_INFO from the list
        m_AvailableMidiPins.erase(item);
    }
    while (TRUE);

    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSMidiEndpointManager::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate update)
{
    //see this function for info on the IDeviceInformationUpdate object: https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices#enumerate-and-watch-devices

    for (auto const& prop : update.Properties())
    {
        OutputDebugString((std::wstring(L"KS: ") + std::wstring(prop.Key().c_str())).c_str() );
    }

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
CMidi2KSMidiEndpointManager::Shutdown()
{
    TraceLoggingWrite(
        MidiKSTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    TransportState::Current().Shutdown();

    m_AvailableMidiPins.clear();

    m_Watcher.Stop();
    m_EnumerationCompleted.wait(500);
    m_DeviceAdded.revoke();
    m_DeviceRemoved.revoke();
    m_DeviceUpdated.revoke();
    m_DeviceStopped.revoke();
    m_DeviceEnumerationCompleted.revoke();

    m_midiDeviceManager.reset();
    m_midiProtocolManager.reset();

    return S_OK;
}
