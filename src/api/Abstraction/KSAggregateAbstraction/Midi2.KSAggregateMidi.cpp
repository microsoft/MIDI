// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"


_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::Initialize(
    LPCWSTR Device,
    MidiFlow Flow,
    PABSTRACTIONCREATIONPARAMS CreationParams,
    DWORD * MmCssTaskId,
    IMidiCallback * Callback,
    LONGLONG Context
)
{
    RETURN_HR_IF(E_INVALIDARG, Flow == MidiFlowIn && nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, Flow == MidiFlowBidirectional && nullptr == Callback);
    RETURN_HR_IF(E_INVALIDARG, nullptr == Device);
    RETURN_HR_IF(E_INVALIDARG, nullptr == MmCssTaskId);
    RETURN_HR_IF(E_INVALIDARG, nullptr == CreationParams);

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initializing", "message"),
        TraceLoggingWideString(Device, "Device"),
        TraceLoggingHexUInt32(CreationParams->DataFormat, "MidiDataFormat"),
        TraceLoggingHexUInt32(Flow, "MidiFlow"),
        TraceLoggingPointer(Callback, "callback")
        );

    wil::unique_handle filter;
    //std::unique_ptr<KSMidiInDevice> midiInDevice;
    //std::unique_ptr<KSMidiOutDevice> midiOutDevice;
    winrt::guid interfaceClass;

    //ULONG inPinId{ 0 };
    //ULONG outPinId{ 0 };
    MidiTransport transportCapabilities;
    MidiTransport transport;

    std::wstring filterInterfaceId;
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId));
    //additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsPinId));
    //additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_InPinId));
    //additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_OutPinId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsTransport));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiGroupPinMap));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(Device, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    filterInterfaceId = winrt::unbox_value<winrt::hstring>(prop).c_str();

    prop = deviceInfo.Properties().Lookup(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    interfaceClass = winrt::unbox_value<winrt::guid>(prop);

    prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_DEVPKEY_KsTransport));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    transportCapabilities = (MidiTransport) winrt::unbox_value<uint32_t>(prop);

    // if creation params specifies MIDI_DATAFORMAT_ANY, the best available transport
    // will be chosen.


    // TODO: This transport format logic is wrong for this type of endpoint.
    // All aggregated devices are byte stream, but THIS device is UMP

    // if creation params specifies that bytestream is requested, then
    // limit the transport to bytestream
    if (CreationParams->DataFormat == MidiDataFormat_ByteStream)
    {
        transportCapabilities = (MidiTransport) ((DWORD) transportCapabilities & (DWORD) MidiTransport_StandardAndCyclicByteStream);
    }
    // if creation params specifies that UMP is requested, then
    // limit the transport to UMP
    else if (CreationParams->DataFormat == MidiDataFormat_UMP)
    {
        transportCapabilities = (MidiTransport) ((DWORD) transportCapabilities & (DWORD) MidiTransport_CyclicUMP);
    }

    // choose the best available transport among the available transports.
    // fill in the MidiDataFormat that is going to be used for the caller.
    if (0 != ((DWORD) transportCapabilities & (DWORD) MidiTransport_CyclicUMP))
    {
        // Cyclic UMP is available, that's the most preferred transport.
        transport = MidiTransport_CyclicUMP;
        CreationParams->DataFormat = MidiDataFormat_UMP;
    }
    else if (0 != ((DWORD) transportCapabilities & (DWORD) MidiTransport_CyclicByteStream))
    {
        // Cyclic bytestream is available, that's the next preferred transport.
        transport = MidiTransport_CyclicByteStream;
        CreationParams->DataFormat = MidiDataFormat_ByteStream;
    }
    else if (0 != ((DWORD) transportCapabilities & (DWORD) MidiTransport_StandardByteStream))
    {
        // Standard bytestream is available, that's the least transport.
        transport = MidiTransport_StandardByteStream;
        CreationParams->DataFormat = MidiDataFormat_ByteStream;
    }
    else
    {
        // invalid/no transport available, error.
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE));
    }


    ULONG requestedBufferSize = PAGE_SIZE;
    RETURN_IF_FAILED(GetRequiredBufferSize(requestedBufferSize));
    RETURN_IF_FAILED(FilterInstantiate(filterInterfaceId.c_str(), &filter));


    if (Flow == MidiFlowBidirectional)
    {
        // Apply pin map here. This will result in multiple KsMidiOutDevice and KsMidiInDevice entries
        // each mapped to a group

        if (!deviceInfo.Properties().HasKey(STRING_DEVPKEY_KsMidiGroupPinMap))
        {
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Endpoint device properties are missing the pin map", "message"),
                TraceLoggingWideString(Device, "device id")
            );

            return E_FAIL;
        }

        auto value = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiGroupPinMap).as<winrt::Windows::Foundation::IPropertyValue>();

        if (value != nullptr)
        {
            auto t = value.Type();

            if (t == winrt::Windows::Foundation::PropertyType::UInt8Array)
            {
                auto refArray = winrt::unbox_value<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiGroupPinMap));

                if (refArray != nullptr)
                {
                    auto data = refArray.Value();

                    auto pinMap = (KSMIDI_PIN_MAP*)(data.data());

                    // process all input group maps
                    for (uint8_t inputGroup = 0; inputGroup < KSMIDI_PIN_MAP_ENTRY_COUNT; inputGroup++)
                    {
                        if (pinMap->InputEntries[inputGroup].IsValid)
                        {
                            // create the KS device and add to our runtime map
                            std::unique_ptr<KSMidiInDevice> device;                               
                            device.reset(new (std::nothrow) KSMidiInDevice());
                            RETURN_IF_NULL_ALLOC(device);

                            RETURN_IF_FAILED(
                                device->Initialize(
                                    Device, 
                                    filter.get(), 
                                    pinMap->InputEntries[inputGroup].PinId, 
                                    transport, 
                                    requestedBufferSize, 
                                    MmCssTaskId, 
                                    Callback, 
                                    Context)
                            );

                            m_midiInDeviceGroupMap[inputGroup] = std::move(device);
                        }
                    }

                    // process all output group maps
                    for (uint8_t outputGroup = 0; outputGroup < KSMIDI_PIN_MAP_ENTRY_COUNT; outputGroup++)
                    {
                        if (pinMap->OutputEntries[outputGroup].IsValid)
                        {
                            // create the KS device and add to our runtime map
                            std::unique_ptr<KSMidiOutDevice> device;
                            device.reset(new (std::nothrow) KSMidiOutDevice());
                            RETURN_IF_NULL_ALLOC(device);

                            RETURN_IF_FAILED(
                                device->Initialize(
                                    Device, 
                                    filter.get(), 
                                    pinMap->OutputEntries[outputGroup].PinId, 
                                    transport, 
                                    requestedBufferSize, 
                                    MmCssTaskId)
                            );

                            m_midiOutDeviceGroupMap[outputGroup] = std::move(device);
                        }
                    }
                }
            }
        }

    }


    return S_OK;
}

HRESULT
CMidi2KSAggregateMidi::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    //if (m_MidiOutDevice)
    //{
    //    m_MidiOutDevice->Cleanup();
    //    m_MidiOutDevice.reset();
    //}
    //if (m_MidiInDevice)
    //{
    //    m_MidiInDevice->Cleanup();
    //    m_MidiInDevice.reset();
    //}

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::SendMidiMessage(
    PVOID Data,
    UINT Length,
    LONGLONG Position
)
{
    // using the group map, so we're a UMP endpoint that is made up of one or 
    // more bytestream MIDI 1.0 endpoints

    if (Length >= sizeof(uint32_t))
    {
        //auto originalWord0 = internal::MidiWordFromVoidPointer(Data);
        auto originalWord0 = *(uint32_t*)Data;

        if (internal::MessageHasGroupField(originalWord0))
        {
            uint8_t groupIndex = internal::GetGroupIndexFromFirstWord(originalWord0);

            auto mapEntry = m_midiOutDeviceGroupMap.find(groupIndex);

            if (mapEntry != m_midiOutDeviceGroupMap.end())
            {
                // translate message before sending to KS device. We use the same
                // code the other transforms use, from the libmidi2 project

                // Send the UMP(s) to the parser
                uint32_t* data = (uint32_t*)Data;
                for (UINT i = 0; i < (Length / sizeof(uint32_t)); i++)
                {
                    m_UMP2BS.UMPStreamParse(data[i]);
                }

                // retrieve the bytestream message from the parser
                // and send it on
                while (m_UMP2BS.availableBS())
                {
                    BYTE byteStream[MAXIMUM_LOOPED_DATASIZE];
                    UINT byteCount;
                    for (byteCount = 0; byteCount < _countof(byteStream) && m_UMP2BS.availableBS(); byteCount++)
                    {
                        byteStream[byteCount] = m_UMP2BS.readBS();
                    }

                    if (byteCount > 0)
                    {
                        RETURN_IF_FAILED(mapEntry->second->SendMidiMessage(&(byteStream[0]), byteCount, Position));
                    }
                }

                return S_OK;
            }
            else
            {
                // invalid group. Dump the message
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    __FUNCTION__,
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"group not found in group to pin map", "message"),
                    TraceLoggingUInt8(groupIndex, "Group Index")
                );
            }

        }
        else
        {
            // it's a message type with no group field. We just drop it and move ok
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                __FUNCTION__,
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Groupless message sent to aggregated KS endpoint", "message")
            );

            return S_OK;
        }
    }
    else
    {
        // this is below the size required for UMP. No idea why we have this. Error.
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            __FUNCTION__,
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"invalid data length", "message"),
            TraceLoggingUInt32(Length, "Length")
        );

        return E_INVALID_PROTOCOL_FORMAT;   // reusing this. Probably should just be E_FAIL
    }

    return E_ABORT;
}

