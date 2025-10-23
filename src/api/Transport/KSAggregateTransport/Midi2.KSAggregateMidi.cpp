// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"

#include "ump_iterator.h"


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
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Initializing", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );


    m_callback = callback;

    //winrt::guid interfaceClass;

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();

    //additionalProperties.Append(STRING_DEVPKEY_KsMidiPort_KsFilterInterfaceId);
    additionalProperties.Append(STRING_DEVPKEY_KsTransport);
    additionalProperties.Append(STRING_DEVPKEY_KsAggMidiGroupPinMap);
 //   additionalProperties.Append(L"System.Devices.InterfaceClassGuid");

    auto deviceInfo = DeviceInformation::CreateFromIdAsync(
        endpointDeviceInterfaceId, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    // we don't actually use a buffer, but this is needed to initialize
    // the KS code which will handle the ioctls
    ULONG requestedBufferSize = PAGE_SIZE;
    RETURN_IF_FAILED(GetRequiredBufferSize(requestedBufferSize));
    

    // Apply pin map here. This will result in multiple KsMidiOutDevice and KsMidiInDevice entries
    // each mapped to a group

    if (!deviceInfo.Properties().HasKey(STRING_DEVPKEY_KsAggMidiGroupPinMap))
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Endpoint device properties are missing the pin map", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        return E_FAIL;
    }

    auto value = deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsAggMidiGroupPinMap).as<winrt::Windows::Foundation::IPropertyValue>();

    if (value == nullptr || value.Type() != winrt::Windows::Foundation::PropertyType::UInt8Array)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to retrieve pin map. Property value was nullptr or incorrect property type.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        RETURN_IF_FAILED(E_FAIL);
    }

    auto refArray = winrt::unbox_value<winrt::Windows::Foundation::IReferenceArray<uint8_t>>(deviceInfo.Properties().Lookup(STRING_DEVPKEY_KsAggMidiGroupPinMap));

    if (refArray == nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to retrieve pin map. Property data was nullptr", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        RETURN_IF_FAILED(E_FAIL);
    }

    auto data = refArray.Value();
    auto pinMap = (KSAGGMIDI_PIN_MAP_PROPERTY_VALUE*)(data.data());

    if (pinMap == nullptr)
    {
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Unable to retrieve pin map. Pinmap was nullptr", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD)
        );

        RETURN_IF_FAILED(E_FAIL);
    }


    auto totalBytes = pinMap->TotalByteCount;
    auto pinMapEntry = (PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY)pinMap->Entries;


    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Pin map retrieved. Processing entries.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
        TraceLoggingUInt32(totalBytes, "Pin map byte count")
    );

    while (pinMapEntry != nullptr && (PBYTE)pinMapEntry < ((PBYTE)pinMap + totalBytes))
    {
        std::wstring filterInterfaceId{ (WCHAR*)pinMapEntry->FilterId };

        std::wstring mapEntryFlow{ };
        switch (pinMapEntry->PinDataFlow)
        {
        case MidiFlow::MidiFlowIn:
            mapEntryFlow = L"Destination";
            break;
        case MidiFlow::MidiFlowOut:
            mapEntryFlow = L"Source";
            break;
        case MidiFlow::MidiFlowBidirectional:
            mapEntryFlow = L"Both";
            break;
        }

        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"Processing pin map entry.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
            TraceLoggingWideString(filterInterfaceId.c_str(), "Filter Id"),
            TraceLoggingUInt32(pinMapEntry->PinId, "Pin"),
            TraceLoggingUInt8(pinMapEntry->GroupIndex, "Group Index"),
            TraceLoggingWideString(mapEntryFlow.c_str(), "Flow Type")
        );

        KsHandleWrapper* filterWrapper = nullptr;

        // Check if we've already opened this filter
        auto it = m_openKsFilters.find(filterInterfaceId);
        if (it != m_openKsFilters.end())
        {
            filterWrapper = it->second.get();
        }
        else
        {
            auto newWrapper = std::make_unique<KsHandleWrapper>(filterInterfaceId);
            RETURN_IF_FAILED(newWrapper->Open());

            auto [insertIt, _] = m_openKsFilters.insert_or_assign(filterInterfaceId, std::move(newWrapper));
            filterWrapper = insertIt->second.get();
        }

        // Duplicate the handle to safely pass it to another component or store it.
        wil::unique_handle handleDupe(filterWrapper->GetHandle());
        RETURN_IF_NULL_ALLOC(handleDupe);

        if (pinMapEntry->PinDataFlow == MidiFlow::MidiFlowOut)
        {
            // pin is a message source, so this is a MIDI In

            wil::com_ptr_nothrow<CMidi2KSAggregateMidiInProxy> proxy;
            RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiInProxy>(&proxy));

            auto initResult =
                proxy->Initialize(
                    endpointDeviceInterfaceId,
                    handleDupe.get(),
                    pinMapEntry->PinId,
                    requestedBufferSize,
                    mmCssTaskId,
                    m_callback,
                    context,
                    pinMapEntry->GroupIndex
                );

            if (SUCCEEDED(initResult))
            {
                m_midiInDeviceGroupMap.insert_or_assign(pinMapEntry->GroupIndex, std::move(proxy));
            }
            else
            {
                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to initialize Midi Input proxy", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                    TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                    TraceLoggingUInt32(pinMapEntry->PinId, "pin id"),
                    TraceLoggingUInt8(pinMapEntry->GroupIndex, "group"),
                    TraceLoggingWideString(filterInterfaceId.c_str(), "filter")
                );

                RETURN_IF_FAILED(initResult);
            }
        }
        else if (pinMapEntry->PinDataFlow == MidiFlow::MidiFlowIn)
        {
            // pin is a message destination, so this is a MIDI Out

            wil::com_ptr_nothrow<CMidi2KSAggregateMidiOutProxy> proxy;
            RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidi2KSAggregateMidiOutProxy>(&proxy));

            auto initResult =
                proxy->Initialize(
                    endpointDeviceInterfaceId,
                    handleDupe.get(),
                    pinMapEntry->PinId,
                    requestedBufferSize,
                    mmCssTaskId,
                    context,
                    pinMapEntry->GroupIndex
                );

            if (SUCCEEDED(initResult))
            {
                m_midiOutDeviceGroupMap.insert_or_assign(pinMapEntry->GroupIndex, std::move(proxy));
            }
            else
            {
                TraceLoggingWrite(
                    MidiKSAggregateTransportTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, "this"),
                    TraceLoggingWideString(L"Unable to initialize Midi Output proxy", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                    TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                    TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                    TraceLoggingUInt32(pinMapEntry->PinId, "pin id"),
                    TraceLoggingUInt8(pinMapEntry->GroupIndex, "group"),
                    TraceLoggingWideString(filterInterfaceId.c_str(), "filter")
                );

                RETURN_IF_FAILED(initResult);
            }

        }
        else
        {
            TraceLoggingWrite(
                MidiKSAggregateTransportTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, "this"),
                TraceLoggingWideString(L"Invalid midi flow", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                TraceLoggingWideString(endpointDeviceInterfaceId, MIDI_TRACE_EVENT_DEVICE_SWD_ID_FIELD),
                TraceLoggingUInt32(requestedBufferSize, "buffer size"),
                TraceLoggingUInt32(pinMapEntry->PinId, "pin id"),
                TraceLoggingUInt8(pinMapEntry->GroupIndex, "group"),
                TraceLoggingWideString(filterInterfaceId.c_str(), "filter")
            );

            RETURN_IF_FAILED(E_FAIL);
        }

        // move on to the next entry
        pinMapEntry = (PKSAGGMIDI_PIN_MAP_PROPERTY_ENTRY)((PBYTE)pinMapEntry + pinMapEntry->ByteCount);
    }


    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
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
        MidiKSAggregateTransportTelemetryProvider::Provider(),
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

    m_openKsFilters.clear();

    return S_OK;
}





_Use_decl_annotations_
HRESULT
CMidi2KSAggregateMidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID inputData,
    UINT length,
    LONGLONG position
)
{
#if _DEBUG
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received UMP message to translate and send", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingHexUInt8Array(static_cast<uint8_t*>(inputData), static_cast<uint16_t>(length), "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#else
    TraceLoggingWrite(
        MidiKSAggregateTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Received UMP message to translate and send", MIDI_TRACE_EVENT_MESSAGE_FIELD),
        TraceLoggingUInt32(static_cast<uint32_t>(optionFlags), "optionFlags"),
        TraceLoggingPointer(inputData, "data"),
        TraceLoggingUInt32(static_cast<uint32_t>(length), "length bytes"),
        TraceLoggingUInt64(static_cast<uint64_t>(position), MIDI_TRACE_EVENT_MESSAGE_TIMESTAMP_FIELD)
    );
#endif

    // We're only capable of handling 1 client at a time
    auto deviceLock = m_SendLock.lock();

    if (length >= sizeof(uint32_t))
    {
        uint32_t pendingWordCount{ 0 };
        bool sendPendingWords{ false };
        uint8_t currentGroupIndex{ 0 };

        internal::UmpBufferIterator bufferIterator(
            static_cast<uint32_t*>(inputData), 
            length / sizeof(uint32_t));

        auto transmissionStart = bufferIterator.begin();
        auto nextTransmissionStart = bufferIterator.begin();

        auto it = bufferIterator.begin();

        // iterate through the list of messages until we get to an incompatible message we need
        // to skip, or we get to the end of the list. To avoid memory allocation from creating a
        // working list, just walk the pointer and  adjust the length as needed and send at any 
        // of these events:
        // - End of the buffer
        // - The Group Index changes
        // - We hit a groupless message
        // - The last message is incomplete

        while (it < bufferIterator.end())
        {
            if (it.CurrentMessageSeemsComplete())
            {
                if (it.CurrentMessageHasGroupField())
                {
                    if (pendingWordCount > 0 && it.CurrentMessageGroupIndex() != currentGroupIndex)
                    {
                        // group index has changed. We need to send what we have and prep a new transmission

                        sendPendingWords = true;
                        nextTransmissionStart = it;

                        // do not increment iterator so we process this next time around
                    }
                    else
                    {
                        pendingWordCount += it.CurrentMessageWordCount();
                        currentGroupIndex = it.CurrentMessageGroupIndex();

                        ++it;
                    }
                }
                else
                {
                    // skip this message
                    ++it;
                    nextTransmissionStart = it;

                    // this message doesn't have a group field, so if we have pending words
                    // send them, and then skip this message
                    if (pendingWordCount > 0)
                    {
                        sendPendingWords = true;
                    }
                    else
                    {
                        transmissionStart = it;
                    }
                }
            }
            else
            {
                ++it;

                if (pendingWordCount > 0)
                {
                    sendPendingWords = true;
                }
                else
                {
                    // break out because invalid message
                    break;
                }
            }

            // force a send if we're at the end of the buffer
            sendPendingWords = sendPendingWords || (it >= bufferIterator.end());

            if (sendPendingWords && pendingWordCount > 0)
            {
                // send the message

                auto mapEntry = m_midiOutDeviceGroupMap.find(currentGroupIndex);

                if (mapEntry != m_midiOutDeviceGroupMap.end())
                {
                    RETURN_IF_FAILED(mapEntry->second->SendMidiMessage(
                        optionFlags, 
                        transmissionStart.get(), 
                        pendingWordCount * sizeof(uint32_t), 
                        position));

                }
                else
                {
                    // invalid group. Dump the message
                    TraceLoggingWrite(
                        MidiKSAggregateTransportTelemetryProvider::Provider(),
                        MIDI_TRACE_EVENT_WARNING,
                        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_WARNING),
                        TraceLoggingPointer(this, "this"),
                        TraceLoggingWideString(L"Group not found in group to pin map. Silently dropping message.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
                        TraceLoggingUInt8(currentGroupIndex, "Group Index from Message")
                    );
                }

                // reset for next iteration
                sendPendingWords = false;
                pendingWordCount = 0;
                transmissionStart = nextTransmissionStart;
            }
        }

        return S_OK;
    }
    else
    {
        // this is below the size required for UMP. No idea why we have this. Error.
        TraceLoggingWrite(
            MidiKSAggregateTransportTelemetryProvider::Provider(),
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
