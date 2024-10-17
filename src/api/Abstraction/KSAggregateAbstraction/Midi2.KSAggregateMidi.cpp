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
//CMidi2KSAggregateMidi::Callback(PVOID data,
//    UINT length,
//    LONGLONG position)
//{
//    // translate from the devices to this
//}



_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::Initialize(
    LPCWSTR endpointDeviceInterfaceId,
    MidiFlow flow,
    PTRANSPORTCREATIONPARAMS /*creationParams*/,
    DWORD * mmCssTaskId,
    IMidiCallback * callback,
    LONGLONG context
)
{
    RETURN_HR_IF(E_INVALIDARG, callback == nullptr);
    RETURN_HR_IF(E_INVALIDARG, mmCssTaskId == nullptr);
    RETURN_HR_IF(E_INVALIDARG, endpointDeviceInterfaceId == nullptr);
    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlow::MidiFlowBidirectional);

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initializing", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );


    m_callback = callback;

    wil::unique_handle filter;
    winrt::guid interfaceClass;

    std::wstring filterInterfaceId;
    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsTransport));
    additionalProperties.Append(winrt::to_hstring(STRING_DEVPKEY_KsMidiGroupPinMap));
    additionalProperties.Append(winrt::to_hstring(L"System.Devices.InterfaceClassGuid"));

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(
        endpointDeviceInterfaceId, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

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
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
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
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return E_FAIL;
    }

    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Grabbing pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                    TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
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
                                endpointDeviceInterfaceId,
                                filter.get(),
                                pinId,
                                requestedBufferSize,
                                mmCssTaskId,
                                m_callback,
                                context,
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
                                TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
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
                                MIDI_TRACE_EVENT_VERBOSE,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Created sub-device proxy for input", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
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
                    MIDI_TRACE_EVENT_VERBOSE,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Processing output entries.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
                );

                // process all output group maps
                for (uint8_t outputGroup = 0; outputGroup < KSMIDI_PIN_MAP_ENTRY_COUNT; outputGroup++)
                {
                    auto pinId = pinMap->OutputEntries[outputGroup].PinId;

                    if (pinMap->OutputEntries[outputGroup].IsValid)
                    {
                        wil::com_ptr_nothrow<CMidi2KSAggregateMidiOutProxy> proxy;
                        RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiOutProxy>(&proxy));

                        auto initResult =
                            proxy->Initialize(
                                endpointDeviceInterfaceId,
                                filter.get(),
                                pinId,
                                requestedBufferSize,
                                mmCssTaskId,
                                context,
                                outputGroup
                            );

                        if (FAILED(initResult))
                        {
                            TraceLoggingWrite(
                                MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                                MIDI_TRACE_EVENT_ERROR,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Unable to initialize sub-device proxy for output", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingHResult(initResult, MIDI_TRACE_EVENT_HRESULT_FIELD),
                                TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
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
                                MIDI_TRACE_EVENT_VERBOSE,
                                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                                TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                                TraceLoggingPointer(this, "this"),
                                TraceLoggingWideString(L"Created sub-device proxy for output", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                                TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                                TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                                TraceLoggingUInt32(pinId, "pin id"),
                                TraceLoggingUInt8(outputGroup, "group")
                            );
                        }

                        m_midiOutDeviceGroupMap[outputGroup] = std::move(proxy);
                    }
                }
            }
        }
    }


    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Completed all initialization", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
    );

    return S_OK;
}

HRESULT
CMidi2KSAggregateMidi::Shutdown()
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Enter", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

    while (!m_midiInDeviceGroupMap.empty())
    {
        auto it = m_midiInDeviceGroupMap.begin();

        LOG_IF_FAILED(it->second->Shutdown());
        it = m_midiInDeviceGroupMap.erase(it);
    }

    while (!m_midiOutDeviceGroupMap.empty())
    {
        auto it = m_midiOutDeviceGroupMap.begin();

        LOG_IF_FAILED(it->second->Shutdown());
        it = m_midiOutDeviceGroupMap.erase(it);
    }

    return S_OK;
}





_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::SendMidiMessage(
    PVOID inputData,
    UINT length,
    LONGLONG position
)
{
    TraceLoggingWrite(
        MidiKSAggregateAbstractionTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received UMP message to translate and send", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(length, "length"),
        TraceLoggingUInt64(position, MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );


    // using the group map, so we're a UMP endpoint that is made up of one or 
    // more bytestream MIDI 1.0 endpoints

    if (length >= sizeof(uint32_t))
    {
        //auto originalWord0 = internal::MidiWordFromVoidPointer(data);
        auto originalWord0 = *(uint32_t*)inputData;

        if (internal::MessageHasGroupField(originalWord0))
        {
            uint8_t groupIndex = internal::GetGroupIndexFromFirstWord(originalWord0);

            auto mapEntry = m_midiOutDeviceGroupMap.find(groupIndex);

            if (mapEntry != m_midiOutDeviceGroupMap.end())
            {
                LOG_IF_FAILED(mapEntry->second->SendMidiMessage(inputData, length, position));

                return S_OK;
            }
            else
            {
                // invalid group. Dump the message
                TraceLoggingWrite(
                    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_WARNING,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Group not found in group to pin map. Dropping message.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingUInt8(groupIndex, "Group Index")
                );

                return S_OK;

            }
        }
        else
        {
            //// it's a message type with no group field. We just drop it and move ok
            //TraceLoggingWrite(
            //    MidiKSAggregateAbstractionTelemetryProvider::Provider(),
            //    MIDI_TRACE_EVENT_INFO,
            //    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            //    TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            //    TraceLoggingPointer(this, "this"),
            //    TraceLoggingWideString(L"Groupless message sent to aggregated KS endpoint. Dropping message.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            //);

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
            TraceLoggingWideString(L"Invalid data length", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingUInt32(length, "length")
        );

        return E_INVALID_PROTOCOL_FORMAT;   // reusing this. Probably should just be E_FAIL
    }

// unreachable
//    return E_ABORT;
}

