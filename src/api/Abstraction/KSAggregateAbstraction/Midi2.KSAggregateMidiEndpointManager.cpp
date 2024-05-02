// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"

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
CMidi2KSAggregateMidiEndpointManager::Initialize(
    IUnknown* midiDeviceManager,
    IUnknown* midiEndpointProtocolManager
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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

    auto deviceAddedHandler = TypedEventHandler<DeviceWatcher, DeviceInformation>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded);
    auto deviceRemovedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved);
    auto deviceUpdatedHandler = TypedEventHandler<DeviceWatcher, DeviceInformationUpdate>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceUpdated);
    auto deviceStoppedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped);
    auto deviceEnumerationCompletedHandler = TypedEventHandler<DeviceWatcher, winrt::Windows::Foundation::IInspectable>(this, &CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted);

    m_DeviceAdded = m_Watcher.Added(winrt::auto_revoke, deviceAddedHandler);
    m_DeviceRemoved = m_Watcher.Removed(winrt::auto_revoke, deviceRemovedHandler);
    m_DeviceUpdated = m_Watcher.Updated(winrt::auto_revoke, deviceUpdatedHandler);
    m_DeviceStopped = m_Watcher.Stopped(winrt::auto_revoke, deviceStoppedHandler);
    m_DeviceEnumerationCompleted = m_Watcher.EnumerationCompleted(winrt::auto_revoke, deviceEnumerationCompletedHandler);

    m_Watcher.Start();
    
    return S_OK;
}





//
// TODO: The OnDeviceAdded handler needs to be broken apart a bit
// Break up pin iteration from device creation. It needs to have separate
// branches for
// - MIDI 2 UMP through a UMP driver
//    - Also create compatible ports
// - MIDI 1.0 byte stream through any driver
//    - Also create the UMP device and fake GTB



// A MIDI device that uses the new UMP driver
// whether it is MIDI 1.0 or MIDI 2.0.
HRESULT
CreateEndpointsForUmpDataFormatDevice(DeviceInformation device, winrt::hstring deviceInstanceId, DeviceInformation parentDevice)
{
    //TraceLoggingWrite(
    //    MidiKSAbstractionTelemetryProvider::Provider(),
    //    __FUNCTION__,
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(device.Id().c_str(), "added device")
    //);

    // After creating the UMP endpoint, needs to kick
    // of protocol negotiation, and then wait to hear
    // back with the function blocks so the compatible
    // MIDI 1.0 ports can be created



    // Create MIDI 1.0 compat ports for a device that uses
    // the new UMP driver, whether it is MIDI 1.0 or MIDI 2.0
    // This needs to map these MIDI 1.0 ports to groups in the endpoint
    // Should ideally work off of function blocks rather than GTBs, if available


    return S_OK;
}


// Create a MIDI 1.0 compat port for a device that
// uses any MIDI 1.0 class compat driver 
HRESULT
CreateEndpointsForMidi1DataFormatDevice(DeviceInformation device, winrt::hstring deviceInstanceId, DeviceInformation parentDevice)
{
    //TraceLoggingWrite(
    //    MidiKSAbstractionTelemetryProvider::Provider(),
    //    __FUNCTION__,
    //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
    //    TraceLoggingPointer(this, "this"),
    //    TraceLoggingWideString(device.Id().c_str(), "added device")
    //);

    // First, this is the simplest approach. We have a MIDI 1.0 device, and we need
    // to create the compatible MIDI 1.0 devices for it. 

    // Create a UMP aggregated endpoint for one or more
    // ports that are exposed via any MIDI 1.0 class 
    // compat driver
    // This needs to create a single aggregated endpoint for the device.
    // Needs to map groups to the pins on the device.
    // Needs to pull in the config file overrides for pin/group mapping

    return S_OK;
}



_Use_decl_annotations_
HRESULT 
CMidi2KSAggregateMidiEndpointManager::OnDeviceAdded(
    DeviceWatcher watcher, 
    DeviceInformation device
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
    RETURN_IF_FAILED(FilterInstantiate(deviceId.c_str(), &hFilter));
    RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), 0, KSPROPSETID_Pin, KSPROPERTY_PIN_CTYPES, &cPins, sizeof(cPins)));

    UINT portNumberDifferentiatorInput{ 0 };
    UINT portNumberDifferentiatorOutput{ 0 };

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

        std::unique_ptr<WCHAR> pinNameData;
        ULONG pinNameDataSize{ 0 };

        UINT16 deviceVID{ 0 };
        UINT16 devicePID{ 0 };

        RETURN_IF_FAILED(PinPropertySimple(hFilter.get(), i, KSPROPSETID_Pin, KSPROPERTY_PIN_COMMUNICATION, &communication, sizeof(KSPIN_COMMUNICATION)));

        // The external connector pin representing the physical connection
        // has KSPIN_COMMUNICATION_NONE. We can only create on software IO pins which
        // have a valid communication. Skip connector pins.
        if (KSPIN_COMMUNICATION_NONE == communication)
        {
            continue;
        }

        // ================== Standard Byte / MIDI 1.0 Interfaces ===============================================
        // attempt to instantiate using standard streaming for UMP buffers
        // and set that flag if we can.
        // NOTE: this has been for bring up performance comparison testing only, and will be removed.
        // (Note from Pete: the above statement doesn't seem correct. This is the branch all MIDI 1.0 devices with the old USB driver fall into)
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_StandardByteStream, &hPin)))
        {
            transportCapability = (MidiTransport)((DWORD) transportCapability | (DWORD) MidiTransport_StandardByteStream);
            dataFormatCapability = (MidiDataFormat) ((DWORD) dataFormatCapability | (DWORD) MidiDataFormat_ByteStream);

            //midiPin->NativeDataFormat = KSDATAFORMAT_SUBTYPE_MIDI;

            // get the name from the Pin. This is how many developers would prefer we name MIDI ports

            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertyAllocate(
                    hFilter.get(), 
                    i, 
                    KSPROPSETID_Pin, 
                    KSPROPERTY_PIN_NAME, 
                    (PVOID*)&pinNameData,
                    &pinNameDataSize),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            if (pinNameDataSize > 0)
            {
                // use the name from the pin. This is the most preferred name for most devices
                deviceName = std::wstring(pinNameData.get());
            }
            else
            {
                // no pin name, so fall back to the interface name with a differentiator

                RETURN_IF_FAILED(
                    PinPropertySimple(
                        hFilter.get(), 
                        i, 
                        KSPROPSETID_Pin, 
                        KSPROPERTY_PIN_DATAFLOW, 
                        &dataFlow, 
                        sizeof(KSPIN_DATAFLOW)));

                if (dataFlow == KSPIN_DATAFLOW_IN)
                {
                    // DATAFLOW_IN is an output port (in to the pin)
                    deviceName = parentDeviceInfo.Name() + L" [Out " + std::to_wstring(portNumberDifferentiatorOutput++) + L"]";
                }
                else
                {
                    deviceName = parentDeviceInfo.Name() + L" [In " + std::to_wstring(portNumberDifferentiatorInput++) + L"]";
                }
            }

            hPin.reset();

            // TEMP!
            //deviceName += L"(MidiSrv)";
        }

        // ================== Cyclic UMP Interfaces ===============================================
        // attempt to instantiate using cyclic streaming for UMP buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_CyclicUMP, &hPin)))
        {
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "added device"),
                TraceLoggingWideString(L"Pin is a Cyclic UMP MIDI pin", "message")
            );

            transportCapability = (MidiTransport)((DWORD) transportCapability |  (DWORD) MidiTransport_CyclicUMP);
            dataFormatCapability = (MidiDataFormat) ((DWORD) dataFormatCapability | (DWORD) MidiDataFormat_UMP);

            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertySimple(hPin.get(), 
                    i, 
                    KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
                    KSPROPERTY_MIDI2_NATIVEDATAFORMAT, 
                    &nativeDataFormat, 
                    sizeof(nativeDataFormat)),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            LOG_IF_FAILED_WITH_EXPECTED(
                PinPropertyAllocate(hPin.get(), 
                    i, 
                    KSPROPSETID_MIDI2_ENDPOINT_INFORMATION, 
                    KSPROPERTY_MIDI2_GROUP_TERMINAL_BLOCKS,
                    (PVOID *)&groupTerminalBlockData, 
                    &groupTerminalBlockDataSize),
                HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            //// Get the serial number
            //LOG_IF_FAILED_WITH_EXPECTED(
            //    PinPropertyAllocate(hPin.get(),
            //        i,
            //        KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //        KSPROPERTY_MIDI2_SERIAL_NUMBER,
            //        (PVOID*)&serialNumberData,
            //        &serialNumberDataSize),
            //    HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));


            //// Get the manufacturer name
            //LOG_IF_FAILED_WITH_EXPECTED(
            //    PinPropertyAllocate(hPin.get(),
            //        i,
            //        KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //        KSPROPERTY_MIDI2_DEVICE_MANUFACTURER,
            //        (PVOID*)&manufacturerNameData,
            //        &manufacturerNameDataSize),
            //    HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            //// VID iVendor
            //LOG_IF_FAILED_WITH_EXPECTED(
            //    PinPropertySimple(hPin.get(),
            //        i,
            //        KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //        KSPROPERTY_MIDI2_DEVICE_VID,
            //        &deviceVID,
            //        sizeof(deviceVID)),
            //    HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            //// PID iProduct
            //LOG_IF_FAILED_WITH_EXPECTED(
            //    PinPropertySimple(hPin.get(),
            //        i,
            //        KSPROPSETID_MIDI2_ENDPOINT_INFORMATION,
            //        KSPROPERTY_MIDI2_DEVICE_PID,
            //        &devicePID,
            //        sizeof(devicePID)),
            //    HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));

            //hPin.reset();
        }

        // ================== Cyclic Byte / MIDI 1.0 Interfaces ===============================================
        // attempt to instantiate using standard streaming for midiOne buffers
        // and set that flag if we can.
        if (SUCCEEDED(InstantiateMidiPin(hFilter.get(), i, MidiTransport_CyclicByteStream, &hPin)))
        {
            transportCapability = (MidiTransport )((DWORD) transportCapability |  (DWORD) MidiTransport_CyclicByteStream);
            dataFormatCapability = ( MidiDataFormat ) ((DWORD) dataFormatCapability | (DWORD) MidiDataFormat_ByteStream);

            //deviceName = L"ZZTestPortName_Cyc";

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
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(device.Id().c_str(), "device id"),
                TraceLoggingWideString(L"No manufacturer name", "manufacturer")
            );
        }

        midiPin->InstanceId = L"MIDIU_KS_";
        midiPin->InstanceId += (midiPin->Flow == MidiFlowOut) ? L"OUT_" : L"IN_";
        
        if (midiPin->SerialNumber.empty())
        {
            midiPin->InstanceId += hash;
        }
        else
        {
            midiPin->InstanceId += midiPin->SerialNumber;
        }

        midiPin->InstanceId += L"_PIN.";
        midiPin->InstanceId += std::to_wstring(midiPin->PinId);

        newMidiPins.push_back(std::move(midiPin));
    }

#ifdef CREATE_KS_BIDI_SWDS

    // UMP devices only. Some redundant checking here, but just in case
    if (newMidiPins.size() == 2 && 
        newMidiPins[0]->Flow != newMidiPins[1]->Flow &&
        (newMidiPins[0]->DataFormatCapability & MidiDataFormat::MidiDataFormat_UMP) == MidiDataFormat::MidiDataFormat_UMP )
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

        midiPin->InstanceId = L"MIDIU_KS_BIDI_";

        if (midiPin->SerialNumber.empty())
        {
            midiPin->InstanceId += hash;
        }
        else
        {
            midiPin->InstanceId += midiPin->SerialNumber;
        }

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
    
    else if ((newMidiPins[0]->DataFormatCapability & MidiDataFormat::MidiDataFormat_ByteStream) == MidiDataFormat::MidiDataFormat_ByteStream)
    {
        // https://github.com/microsoft/MIDI/issues/184


        // create an aggregated UMP device from existing MIDI 1.0 devices

        //        auto midiInPinIndex = newMidiPins[0]->Flow == MidiFlowIn ? 0 : 1;
//        auto midiOutPinIndex = newMidiPins[0]->Flow == MidiFlowOut ? 0 : 1;

        // create a new bidi entry
        std::unique_ptr<MIDI_PIN_INFO> midiPin = std::make_unique<MIDI_PIN_INFO>();
        RETURN_IF_NULL_ALLOC(midiPin);

        // build out the pin map
        // TODO: Check for preferred/remapping in config file

        uint8_t currentInputGroup{ 0 };
        uint8_t currentOutputGroup{ 0 };

        for (auto &pin : newMidiPins)
        {
            if (pin->Flow == MidiFlow::MidiFlowIn)
            {
                midiPin->PinMap.InputEntries[currentInputGroup].IsValid = true;
                midiPin->PinMap.InputEntries[currentInputGroup].PinId = pin->PinId;
                currentInputGroup++;
            }
            else if(pin->Flow == MidiFlow::MidiFlowOut)
            {
                midiPin->PinMap.OutputEntries[currentOutputGroup].IsValid = true;
                midiPin->PinMap.OutputEntries[currentOutputGroup].PinId = pin->PinId;
                currentOutputGroup++;
            }
            else
            {
                // should be no bidi in here. This is unexpected.
            }           
        }

        midiPin->UsePinMap = true;

        // the aggregated endpoint is UMP because we need access to the group info. Translation happens inside.
        midiPin->TransportCapability = MidiTransport::MidiTransport_CyclicUMP;
        midiPin->DataFormatCapability = MidiDataFormat::MidiDataFormat_UMP; 

        midiPin->Name = deviceName;
        midiPin->Id = deviceId;
        midiPin->ParentInstanceId = deviceInstanceId;

        midiPin->SerialNumber = newMidiPins[0]->SerialNumber;
        midiPin->ManufacturerName = newMidiPins[0]->ManufacturerName;

        // TODO: Create fake Group Terminal blocks for the pin map
        // Those GTBs need the names we identify here
        
        midiPin->NativeDataFormat = KSDATAFORMAT_SUBTYPE_MIDI;
        midiPin->Flow = MidiFlowBidirectional;

        midiPin->InstanceId = L"MIDIU_KS_BIDI_";

        if (midiPin->SerialNumber.empty())
        {
            midiPin->InstanceId += hash;
        }
        else
        {
            midiPin->InstanceId += midiPin->SerialNumber;
        }

        midiPin->InstanceId += L"_MULTPINS";

//        midiPin->InstanceId += L"_OUTPIN.";
//        midiPin->InstanceId += std::to_wstring(midiPin->PinId);
//        midiPin->InstanceId += L"_INPIN.";
//        midiPin->InstanceId += std::to_wstring(midiPin->PinIdIn);
//
#ifdef BIDI_REPLACES_INOUT_SWDS
        // replace the existing entries with this one bidi entry
        newMidiPins.clear();
#else
        // don't want to double the legacy SWD's, so if we are
        // creating both in and out swd's, no need to duplicate
        // bidirectional as well.
        midiPin->CreateUMPOnly = TRUE;
#endif
//        newMidiPins.push_back(std::move(midiPin));

    }
#endif


    //in the added callback we're going to go ahead and create the SWD's:
    for (auto const& MidiPin : newMidiPins)
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(MidiPin->Name.c_str(), "pin name"),
            TraceLoggingWideString(L"creating endpoint from pin", "message")
        );

        GUID KsAbstractionLayerGUID = __uuidof(Midi2KSAggregateAbstraction);
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
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin native data format is UMP", "message"),
                    TraceLoggingWideString(MidiPin->Name.c_str(), "pin name")
                );

                // default native UMP devices to support MIDI 1.0 and MIDI 2.0 until they negotiation otherwise
                // These properties were originally meant only for the results of protocol negotiation, but they were confusing when not set

                commonProperties.NativeDataFormat = MIDI_PROP_NATIVEDATAFORMAT_UMP;
                commonProperties.RequiresMetadataHandler = true;
                commonProperties.SupportsMidi1ProtocolDefaultValue = true;
                commonProperties.SupportsMidi2ProtocolDefaultValue = true;
            }
            else if (MidiPin->NativeDataFormat == KSDATAFORMAT_SUBTYPE_MIDI)
            {
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(MidiPin->Name.c_str(), "pin name"),
                    TraceLoggingWideString(L"Pin native data format is bytestream", "message")
                );

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
        if (MidiPin->Flow == MidiFlowBidirectional && !MidiPin->UsePinMap)
        {
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(MidiPin->Name.c_str(), "pin name"),
                TraceLoggingWideString(L"Pin is BiDi and does not use a group/pin map", "message")
            );

            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_OutPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinId) });
            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiPort_InPinId, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_UINT32, static_cast<ULONG>(sizeof(UINT32)), &(MidiPin->PinIdIn) });
        }
        else if (MidiPin->Flow == MidiFlowBidirectional && MidiPin->UsePinMap)
        {
            // Bidirectional uses a different property for the in and out pins, since we currently require two separate ones.
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(MidiPin->Name.c_str(), "pin name"),
                TraceLoggingWideString(L"Pin is bidi and uses a group/pin map", "message")
            );

            interfaceDevProperties.push_back({ { DEVPKEY_KsMidiGroupPinMap, DEVPROP_STORE_SYSTEM, nullptr },
                DEVPROP_TYPE_BINARY, sizeof(MidiPin->PinMap), &(MidiPin->PinMap) });
        }
        else
        {
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(MidiPin->Name.c_str(), "pin name"),
                TraceLoggingWideString(L"Pin is regular unidirectional MIDI pin", "message")
            );

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
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(jsonSearchKeys.c_str(), "jsonSearchKeys")
            );

            auto applyConfigHR = AbstractionState::Current().GetConfigurationManager()->ApplyConfigFileUpdatesForEndpoint(jsonSearchKeys);

            if (FAILED(applyConfigHR))
            {
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
HRESULT CMidi2KSAggregateMidiEndpointManager::OnDeviceRemoved(DeviceWatcher, DeviceInformationUpdate device)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
HRESULT CMidi2KSAggregateMidiEndpointManager::OnDeviceUpdated(DeviceWatcher, DeviceInformationUpdate)
{
    //see this function for info on the IDeviceInformationUpdate object: https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/enumerate-devices#enumerate-and-watch-devices
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSAggregateMidiEndpointManager::OnDeviceStopped(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}

_Use_decl_annotations_
HRESULT CMidi2KSAggregateMidiEndpointManager::OnEnumerationCompleted(DeviceWatcher, winrt::Windows::Foundation::IInspectable)
{
    m_EnumerationCompleted.SetEvent();
    return S_OK;
}



HRESULT
CMidi2KSAggregateMidiEndpointManager::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
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
