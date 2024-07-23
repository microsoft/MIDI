// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"


//_Use_decl_annotations_
//HRESULT
//CMidi2KSAggregateMidi::Callback(PVOID Data,
//    UINT Length,
//    LONGLONG Position)
//{
//    // translate from the devices to this
//}





_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::Initialize(
    LPCWSTR Device,
    MidiFlow Flow,
    PABSTRACTIONCREATIONPARAMS /*CreationParams*/,
    DWORD * MmCssTaskId,
    IMidiCallback * Callback,
    LONGLONG Context
)
{
    RETURN_HR_IF(E_INVALIDARG, Callback == nullptr);
    RETURN_HR_IF(E_INVALIDARG, Device == nullptr);
    RETURN_HR_IF(E_INVALIDARG, Flow != MidiFlow::MidiFlowBidirectional);

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initializing", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(Device, "Device")
        );


    m_callback = Callback;

    wil::unique_handle filter;
    winrt::guid interfaceClass;

    std::wstring filterInterfaceId;
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsTransport));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiGroupPinMap));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(
        Device, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    filterInterfaceId = winrt::unbox_value<winrt::hstring>(prop).c_str();

    prop = deviceInfo.Properties().Lookup(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));
    RETURN_HR_IF_NULL(E_INVALIDARG, prop);
    interfaceClass = winrt::unbox_value<winrt::guid>(prop);

    ULONG requestedBufferSize = PAGE_SIZE * 2;
    RETURN_IF_FAILED(GetRequiredBufferSize(requestedBufferSize));
    RETURN_IF_FAILED(FilterInstantiate(filterInterfaceId.c_str(), &filter));


    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Retrieved properties", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(filterInterfaceId.c_str(), "filter interface id"),
        TraceLoggingGuid(interfaceClass, "interfaceClass")
    );

    // Apply pin map here. This will result in multiple KsMidiOutDevice and KsMidiInDevice entries
    // each mapped to a group

    if (!deviceInfo.Properties().HasKey(STRING_DEVPKEY_KsMidiGroupPinMap))
    {
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint device properties are missing the pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(Device, "device id")
        );

        return E_FAIL;
    }

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Grabbing pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(Device, "device id")
    );

    auto value = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiGroupPinMap).as<winrt::Windows::Foundation::IPropertyValue>();

    if (value != nullptr)
    {
        auto t = value.Type();

        if (t == winrt::Windows::Foundation::PropertyType::UInt8Array)
        {
            auto refArray = winrt::unbox_value<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsMidiGroupPinMap));

            if (refArray != nullptr)
            {
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Pin map retrieved. Processing input entries.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(Device, "device id")
                );

                auto data = refArray.Value();

                auto pinMap = (KSMIDI_PIN_MAP*)(data.data());

                // process all input group maps
                for (uint8_t inputGroup = 0; inputGroup < KSMIDI_PIN_MAP_ENTRY_COUNT; inputGroup++)
                {
                    auto pinId = pinMap->InputEntries[inputGroup].PinId;

                    if (pinMap->InputEntries[inputGroup].IsValid)
                    {
                        wil::com_ptr_nothrow<CMidi2KSAggregateMidiInProxy> proxy;
                        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiInProxy>(&proxy));

                        auto initResult =
                            proxy->Initialize(
                                Device,
                                filter.get(),
                                pinId,
                                requestedBufferSize,
                                MmCssTaskId,
                                m_callback,
                                Context,
                                inputGroup
                            );

                        if (FAILED(initResult))
                        {
                            TraceLoggingWrite(
                                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unable to initialize sub-device proxy for input", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingHResult(initResult, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                TraceLoggingWideString(Device, "device id"),
                                TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                                TraceLoggingUInt32(pinId, "pin id"),
                                TraceLoggingUInt8(inputGroup, "group")
                            );

                            RETURN_IF_FAILED(initResult);
                        }
                        else
                        {
                            TraceLoggingWrite(
                                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Created sub-device proxy for input", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(Device, "device id"),
                                TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                                TraceLoggingUInt32(pinId, "pin id"),
                                TraceLoggingUInt8(inputGroup, "group")
                            );
                        }

                        m_midiInDeviceGroupMap[inputGroup] = std::move(proxy);
                    }
                }

                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Processing output entries.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(Device, "device id")
                );

                // process all output group maps
                for (uint8_t outputGroup = 0; outputGroup < KSMIDI_PIN_MAP_ENTRY_COUNT; outputGroup++)
                {
                    auto pinId = pinMap->OutputEntries[outputGroup].PinId;
                        
                    if (pinMap->OutputEntries[outputGroup].IsValid)
                    {
                        // create the KS device and add to our runtime map
                        auto device = std::make_unique<KSMidiOutDevice>();
                        RETURN_IF_NULL_ALLOC(device);

                        auto initResult =
                            device->Initialize(
                                Device, 
                                filter.get(), 
                                pinId, 
                                MidiTransport::MidiTransport_StandardByteStream,
                                requestedBufferSize, 
                                MmCssTaskId
                            );

                        if (FAILED(initResult))
                        {
                            TraceLoggingWrite(
                                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unable to initialize sub-device for output", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingHResult(initResult, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                TraceLoggingWideString(Device, "device id"),
                                TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                                TraceLoggingUInt32(pinId, "pin id"),
                                TraceLoggingUInt8(outputGroup, "group")
                            );

                            RETURN_IF_FAILED(initResult);
                        }
                        else
                        {
                            TraceLoggingWrite(
                                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_INFO,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Created sub-device for output", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(Device, "device id"),
                                TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                                TraceLoggingUInt32(pinId, "pin id"),
                                TraceLoggingUInt8(outputGroup, "group")
                            );
                        }

                        m_midiOutDeviceGroupMap[outputGroup] = std::move(device);
                    }
                }
            }
        }
    }


    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Completed all initialization", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(Device, "device id")
    );

    return S_OK;
}

HRESULT
CMidi2KSAggregateMidi::Cleanup()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
        );

    for (auto it = m_midiInDeviceGroupMap.begin(); it != m_midiInDeviceGroupMap.end(); it++)
    {
        LOG_IF_FAILED(it->second->Cleanup());
        m_midiInDeviceGroupMap.erase(it);
    }

    for (auto it = m_midiOutDeviceGroupMap.begin(); it != m_midiOutDeviceGroupMap.end(); it++)
    {
        LOG_IF_FAILED(it->second->Cleanup());
        m_midiOutDeviceGroupMap.erase(it);
    }

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
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received UMP message to translate and send", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(Length, "length")
    );


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
                        auto sendHr = mapEntry->second->SendMidiMessage(&(byteStream[0]), byteCount, Position);

                        if (FAILED(sendHr))
                        {
                            // iunable to send message
                            TraceLoggingWrite(
                                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unable to send message to MIDI 1.0 endpoint.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingUInt8(groupIndex, "Group Index"),
                                TraceLoggingHResult(sendHr),
                                TraceLoggingUInt8Array(&(byteStream[0]), (UINT16)byteCount, "message bytes")
                            );

                            LOG_IF_FAILED(sendHr);
                        }
                    }
                }

                return S_OK;
            }
            else
            {
                // invalid group. Dump the message
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_INFO,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Group not found in group to pin map. Dropping message.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(groupIndex, "Group Index")
                );
            }

        }
        else
        {
            // it's a message type with no group field. We just drop it and move ok
            TraceLoggingWrite(
                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Groupless message sent to aggregated KS endpoint. Dropping message.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return S_OK;
        }
    }
    else
    {
        // this is below the size required for UMP. No idea why we have this. Error.
        TraceLoggingWrite(
            MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"invalid data length", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(Length, "Length")
        );

        return E_INVALID_PROTOCOL_FORMAT;   // reusing this. Probably should just be E_FAIL
    }

    return E_ABORT;
}

