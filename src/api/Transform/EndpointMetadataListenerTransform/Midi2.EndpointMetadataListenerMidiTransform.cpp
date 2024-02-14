// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


DEVPROPKEY FunctionBlockPropertyKeyFromNumber(_In_ uint8_t functionBlockNumber)
{
    DEVPROPKEY key{};
    GUID propertyId{};

    if (functionBlockNumber < MIDI_MAX_FUNCTION_BLOCKS)
    {
        if (SUCCEEDED(::CLSIDFromString(MIDI_STRING_PKEY_GUID, &propertyId)))
        {
            key.fmtid = propertyId;
            key.pid = MIDI_FUNCTION_BLOCK_PROPERTY_INDEX_START + functionBlockNumber;
        }
        else
        {
            OutputDebugString(L"CLSIDFromString failed for Function Block Property Key");
        }
    }

    return key;
}

DEVPROPKEY FunctionBlockNamePropertyKeyFromNumber(_In_ uint8_t functionBlockNumber)
{
    DEVPROPKEY key{};
    GUID propertyId{};

    if (functionBlockNumber < MIDI_MAX_FUNCTION_BLOCKS)
    {
        if (SUCCEEDED(::CLSIDFromString(MIDI_STRING_PKEY_GUID, &propertyId)))
        {
            key.fmtid = propertyId;
            key.pid = MIDI_FUNCTION_BLOCK_NAME_PROPERTY_INDEX_START + functionBlockNumber;
        }
    }

    return key;
}







_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::Initialize(
    LPCWSTR deviceId,
    PTRANSFORMCREATIONPARAMS creationParams,
    DWORD* mmcssTaskId,
    IMidiCallback* callback,
    LONGLONG context,
    IUnknown* MidiDeviceManager
)
{
    UNREFERENCED_PARAMETER(creationParams);
    UNREFERENCED_PARAMETER(mmcssTaskId);

    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, MidiDeviceManager);
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    RETURN_IF_FAILED(MidiDeviceManager->QueryInterface(__uuidof(IMidiDeviceManagerInterface), (void**)&m_MidiDeviceManager));

    m_callback = callback;

    m_deviceInstanceId = deviceId;
    m_context = context;


    std::thread workerThread(
        &CMidi2EndpointMetadataListenerMidiTransform::QueueWorker,
        this);

    // TODO: may need to set thread priority
    //SetThreadPriority(workerThread.native_handle(), ... );

    m_queueWorkerThread = std::move(workerThread);

    // start up the worker thread
    m_queueWorkerThread.detach();


    return S_OK;
}

HRESULT
CMidi2EndpointMetadataListenerMidiTransform::Cleanup()
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    m_continueProcessing = false;
    m_messageProcessorWakeup.SetEvent();

    // join the worker thread and wait for it to end
    if (m_queueWorkerThread.joinable())
        m_queueWorkerThread.join();



    return S_OK;

}


#define MIDI_ENDPOINT_METADATA_LISTENER_THREAD_SLEEP_DURATION_MS 2000

void CMidi2EndpointMetadataListenerMidiTransform::QueueWorker()
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Entering Worker", "message")
    );

    while (m_continueProcessing)
    {
        // sleep if the queue is empty
        if (m_workQueue.size() == 0)
        {
            // we're in a loop. Before sleeping, check to 
            // see if we're signaled from before
            if (m_messageProcessorWakeup.is_signaled())
            {
                m_messageProcessorWakeup.ResetEvent();
            }

            m_messageProcessorWakeup.wait(MIDI_ENDPOINT_METADATA_LISTENER_THREAD_SLEEP_DURATION_MS);
        }

        // actually process any data we have
        while (m_continueProcessing && m_workQueue.size() > 0)
        {
            m_queueMutex.lock();

            auto ump = m_workQueue.front();
            m_workQueue.pop();

            m_queueMutex.unlock();

            ProcessStreamMessage(ump);
        }

    }

    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(L"Exiting Worker", "message")
    );
}





_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::SendMidiMessage(
    PVOID data,
    UINT size,
    LONGLONG timestamp)
{
    // TODO: This really should have a worker thread doing the message processing
    // because we don't want to add any latency or jitter here. This prototype will
    // be all in-line, however.

    RETURN_HR_IF_NULL(E_INVALIDARG, data);

    // Forward immediately. The client receives all messages, even if we handle them here
    if (m_callback)
    {
        m_callback->Callback(data, size, timestamp, m_context);
    }


    // now that we're back from the callback, handle the message

    if (size == UMP128_BYTE_COUNT)
    {
        internal::PackedUmp128 ump;

        if (internal::FillPackedUmp128FromBytePointer((byte*)data, (uint8_t)size, ump))
        {
            // if type F, process it.

            if (internal::GetUmpMessageTypeFromFirstWord(ump.word0) == 0xF)
            {
                m_queueMutex.lock();
                m_workQueue.push(ump);
                m_queueMutex.unlock();
            }
            else
            {
                // not a stream message. Ignore and move on
            }
        }
        else
        {
            // couldn't fill the UMP. Shouldn't happen since we pre-validate. Just ignore.
            //return E_FAIL;
        }
    }
    else
    {
        // Not a UMP128 so can't be a stream message. Fall out quickly
    }

    return S_OK;
}



HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateEndpointNameProperty()
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    //OutputDebugString(L"\n" __FUNCTION__ " endpoint name is: ");
    //OutputDebugString(m_endpointName.c_str());
    //OutputDebugString(L"\n");

    auto cleanedValue{ internal::TrimmedWStringCopy(m_endpointName) + L"\0" };

    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointProvidedName, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    // clear out any old value that's in there
    //m_endpointName.clear();

    return S_OK;
}

HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateEndpointProductInstanceIdProperty()
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    //OutputDebugString(L"\n" __FUNCTION__ " product instance id is: '");
    //OutputDebugString(m_productInstanceId.c_str());
    //OutputDebugString(L"'\n");

    std::wstring cleanedValue{ internal::TrimmedWStringCopy(m_productInstanceId) + L"\0" };

    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointProvidedProductInstanceId, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    // clear out any old value that's in there
    //m_productInstanceId.clear();

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateFunctionBlockNameProperty(uint8_t functionBlockNumber, std::wstring name)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    //OutputDebugString(L"\n" __FUNCTION__ " function block name is: ");
    //OutputDebugString(name.c_str());
    //OutputDebugString(L"\n");

    std::wstring cleanedValue{ internal::TrimmedWStringCopy(name) + L"\0" };

    DEVPROPERTY props[] =
    {
        {{ FunctionBlockNamePropertyKeyFromNumber(functionBlockNumber), DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_STRING, static_cast<ULONG>((cleanedValue.length() + 1) * sizeof(WCHAR)), (PVOID)(cleanedValue.c_str()) },
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    // clear out any old name that's in there
  //  m_functionBlockNames.erase(functionBlockNumber);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2EndpointMetadataListenerMidiTransform::UpdateStreamConfigurationProperties(internal::PackedUmp128& endpointStreamConfigurationNotificationMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    BYTE configuredProtocol = MIDIWORDBYTE3(endpointStreamConfigurationNotificationMessage.word0);

    DEVPROP_BOOLEAN configuredToSendJR = MIDIWORDBYTE4LOWBIT1(endpointStreamConfigurationNotificationMessage.word0) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN configuredToReceiveJR = MIDIWORDBYTE4LOWBIT2(endpointStreamConfigurationNotificationMessage.word0) ? DEVPROP_TRUE : DEVPROP_FALSE;
    
    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointConfiguredToSendJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(configuredToSendJR)), &configuredToSendJR },

        {{ PKEY_MIDI_EndpointConfiguredToReceiveJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(configuredToReceiveJR)), &configuredToReceiveJR },

        {{ PKEY_MIDI_EndpointConfiguredProtocol, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(configuredProtocol)), &configuredProtocol },
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    //OutputDebugString(L"" __FUNCTION__ " Properties Updated");

    return S_OK;
}



// The device identity is a single message, so we don't keep a copy. Instead, we just do
// the parsing here and then update the property. We assume this has already been identified
// as a type=F, form=0, status=0x02 stream message
_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateDeviceIdentityProperty(internal::PackedUmp128& identityMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    MidiDeviceIdentityProperty prop;

    prop.Reserved0 = MIDIWORDSHORT2(identityMessage.word0);

    prop.Reserved1 = internal::CleanupByte7(MIDIWORDBYTE1(identityMessage.word1));
    prop.ManufacturerSysExIdByte1 = internal::CleanupByte7(MIDIWORDBYTE2(identityMessage.word1));
    prop.ManufacturerSysExIdByte2 = internal::CleanupByte7(MIDIWORDBYTE3(identityMessage.word1));
    prop.ManufacturerSysExIdByte3 = internal::CleanupByte7(MIDIWORDBYTE4(identityMessage.word1));

    prop.DeviceFamilyLsb = internal::CleanupByte7(MIDIWORDBYTE1(identityMessage.word2));
    prop.DeviceFamilyMsb = internal::CleanupByte7(MIDIWORDBYTE2(identityMessage.word2));
    prop.DeviceFamilyModelNumberLsb = internal::CleanupByte7(MIDIWORDBYTE3(identityMessage.word2));
    prop.DeviceFamilyModelNumberMsb = internal::CleanupByte7(MIDIWORDBYTE4(identityMessage.word2));

    prop.SoftwareRevisionLevelByte1 = internal::CleanupByte7(MIDIWORDBYTE1(identityMessage.word3));
    prop.SoftwareRevisionLevelByte2 = internal::CleanupByte7(MIDIWORDBYTE2(identityMessage.word3));
    prop.SoftwareRevisionLevelByte3 = internal::CleanupByte7(MIDIWORDBYTE3(identityMessage.word3));
    prop.SoftwareRevisionLevelByte4 = internal::CleanupByte7(MIDIWORDBYTE4(identityMessage.word3));


    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_DeviceIdentity, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), &prop },
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    //OutputDebugString(L"" __FUNCTION__ " Property Updated");


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateEndpointInfoProperties(internal::PackedUmp128& endpointInfoNotificationMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    BYTE umpVersionMajor = MIDIWORDBYTE3(endpointInfoNotificationMessage.word0);
    BYTE umpVersionMinor = MIDIWORDBYTE4(endpointInfoNotificationMessage.word0);
    BYTE functionBlockCount = internal::CleanupByte7(MIDIWORDBYTE1(endpointInfoNotificationMessage.word1));

    DEVPROP_BOOLEAN functionBlocksAreStatic = MIDIWORDHIGHBIT(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROP_BOOLEAN supportsMidi1Protocol = MIDIWORDBYTE3LOWBIT1(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN supportsMidi2Protocol = MIDIWORDBYTE3LOWBIT2(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;

    DEVPROP_BOOLEAN supportsSendingJR = MIDIWORDBYTE4LOWBIT1(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;
    DEVPROP_BOOLEAN supportsReceivingJR = MIDIWORDBYTE4LOWBIT2(endpointInfoNotificationMessage.word1) ? DEVPROP_TRUE : DEVPROP_FALSE;


    DEVPROPERTY props[] =
    {
        {{ PKEY_MIDI_EndpointSupportsMidi2Protocol, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsMidi2Protocol)), &supportsMidi2Protocol },

        {{ PKEY_MIDI_EndpointSupportsMidi1Protocol, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsMidi1Protocol)), &supportsMidi1Protocol },

        {{ PKEY_MIDI_EndpointSupportsReceivingJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsReceivingJR)), &supportsReceivingJR },

        {{ PKEY_MIDI_EndpointSupportsSendingJRTimestamps, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(supportsSendingJR)), &supportsSendingJR },

        {{ PKEY_MIDI_EndpointUmpVersionMajor, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(umpVersionMajor)), &umpVersionMajor },

        {{ PKEY_MIDI_EndpointUmpVersionMinor, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(umpVersionMajor)), & umpVersionMinor },

        {{ PKEY_MIDI_FunctionBlockCount, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BYTE, static_cast<ULONG>(sizeof(functionBlockCount)), &functionBlockCount },

        {{ PKEY_MIDI_FunctionBlocksAreStatic, DEVPROP_STORE_SYSTEM, nullptr },
            DEVPROP_TYPE_BOOLEAN, static_cast<ULONG>(sizeof(functionBlocksAreStatic)), &functionBlocksAreStatic },
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    //OutputDebugString(L"" __FUNCTION__ " Properties Updated");


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateFunctionBlockProperty(internal::PackedUmp128& functionBlockInfoNotificationMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    MidiFunctionBlockProperty prop;

    prop.IsActive = MIDIWORDBYTE3HIGHBIT(functionBlockInfoNotificationMessage.word0);
    prop.BlockNumber = internal::CleanupByte7(MIDIWORDBYTE3(functionBlockInfoNotificationMessage.word0));
    prop.Direction = MIDIWORDBYTE4LOWCRUMB1(functionBlockInfoNotificationMessage.word0);
    prop.Midi1 = MIDIWORDBYTE4LOWCRUMB2(functionBlockInfoNotificationMessage.word0);
    prop.UIHint = MIDIWORDBYTE4LOWCRUMB3(functionBlockInfoNotificationMessage.word0);
    prop.Reserved0 = MIDIWORDBYTE4LOWCRUMB4(functionBlockInfoNotificationMessage.word0);

    prop.FirstGroup = MIDIWORDBYTE1(functionBlockInfoNotificationMessage.word1);
    prop.NumberOfGroupsSpanned = MIDIWORDBYTE2(functionBlockInfoNotificationMessage.word1);
    prop.MidiCIMessageVersionFormat = MIDIWORDBYTE3(functionBlockInfoNotificationMessage.word1);
    prop.MaxSysEx8Streams = MIDIWORDBYTE4(functionBlockInfoNotificationMessage.word1);

    prop.Reserved1 = functionBlockInfoNotificationMessage.word2;
    prop.Reserved2 = functionBlockInfoNotificationMessage.word3;


    DEVPROPKEY propKey = FunctionBlockPropertyKeyFromNumber(prop.BlockNumber);

    DEVPROPERTY props[] =
    {
        {{ propKey, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), &prop },
    };


    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2EndpointMetadataListenerMidiTransform::ProcessStreamMessage(internal::PackedUmp128 ump)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    auto messageStatus = internal::GetStatusFromStreamMessageFirstWord(ump.word0);
    
    switch (messageStatus)
    {
    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_INFO_NOTIFICATION\n");
        UpdateEndpointInfoProperties(ump);
        break;
    case MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_DEVICE_IDENTITY_NOTIFICATION\n");
        UpdateDeviceIdentityProperty(ump);
        break;
    case MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_STREAM_CONFIGURATION_NOTIFICATION\n");
        UpdateStreamConfigurationProperties(ump);
        break;
    case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_INFO_NOTIFICATION\n");
        UpdateFunctionBlockProperty(ump);
        break;
    case MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION\n");
        HandleFunctionBlockNameMessage(ump);
        break;
    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_PRODUCT_INSTANCE_ID_NOTIFICATION\n");
        HandleProductInstanceIdMessage(ump);
        break;
    case MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION:
        OutputDebugString(L" MIDI_STREAM_MESSAGE_STATUS_ENDPOINT_NAME_NOTIFICATION\n");
        HandleEndpointNameMessage(ump);
        break;
    default:
        OutputDebugString(L" Message is unidentified stream message\n");
        break;
    }

    return S_OK;
}

std::wstring ParseStreamTextMessage(_In_ internal::PackedUmp128& message)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );


    // check the status to know which byte is first to be grabbed

    // preallocate a wstring to the maximum length for a single message to avoid reallocation. Fill with zero

    uint8_t maxCharsThisMessage{ 14 };     // TODO

    uint16_t messageStatus = internal::GetStatusFromStreamMessageFirstWord(message.word0);

    if (messageStatus == MIDI_STREAM_MESSAGE_STATUS_FUNCTION_BLOCK_NAME_NOTIFICATION)
    {
        // function block name messages have 13 character bytes instead of 14 due to having the FB number in the message
        maxCharsThisMessage = 13;
    }
    else
    {
        maxCharsThisMessage = 14;
    }

    std::wstring text;
    text.reserve(maxCharsThisMessage+1);  // try to avoid reallocations


    if (maxCharsThisMessage == 14)
    {
        if (MIDIWORDBYTE3(message.word0) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word0));
    }

    // this unroll is easier than looping, honestly.
    // Also, the property set completely fails if there are any embedded nuls, so need to
    // ignore any in the source data
    if (MIDIWORDBYTE4(message.word0) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word0));

    if (MIDIWORDBYTE1(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE1(message.word1));
    if (MIDIWORDBYTE2(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE2(message.word1));
    if (MIDIWORDBYTE3(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word1));
    if (MIDIWORDBYTE4(message.word1) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word1));

    if (MIDIWORDBYTE1(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE1(message.word2));
    if (MIDIWORDBYTE2(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE2(message.word2));
    if (MIDIWORDBYTE3(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word2));
    if (MIDIWORDBYTE4(message.word2) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word2));

    if (MIDIWORDBYTE1(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE1(message.word3));
    if (MIDIWORDBYTE2(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE2(message.word3));
    if (MIDIWORDBYTE3(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE3(message.word3));
    if (MIDIWORDBYTE4(message.word3) != 0) text.push_back((wchar_t)MIDIWORDBYTE4(message.word3));

    text.shrink_to_fit();

    return text;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::HandleFunctionBlockNameMessage(internal::PackedUmp128& functionBlockNameMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    uint8_t functionBlockNumber = MIDIWORDBYTE3(functionBlockNameMessage.word0);

    switch (internal::GetFormFromStreamMessageFirstWord(functionBlockNameMessage.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, ParseStreamTextMessage(functionBlockNameMessage)));
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
        {
            std::wstring name = ParseStreamTextMessage(functionBlockNameMessage);

            m_functionBlockNames.insert_or_assign(functionBlockNumber, name);
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (m_functionBlockNames.find(functionBlockNumber) != m_functionBlockNames.end())
        {
            std::wstring name = m_functionBlockNames.find(functionBlockNumber)->second;
            name += ParseStreamTextMessage(functionBlockNameMessage);

            m_functionBlockNames.insert_or_assign(functionBlockNumber, name);
        }
        else
        {
            // name isn't already in the map, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (m_functionBlockNames.find(functionBlockNumber) != m_functionBlockNames.end())
        {
            std::wstring name = m_functionBlockNames.find(functionBlockNumber)->second;
            name += ParseStreamTextMessage(functionBlockNameMessage);

            RETURN_IF_FAILED(UpdateFunctionBlockNameProperty(functionBlockNumber, name));
        }
        else
        {
            // name isn't already in the map, so at least start message was skipped. Don't append anything at all.
        }
        break;

    default:
        // won't actually happen because the Form field is only 2 bits
        break;
    }

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::HandleEndpointNameMessage(internal::PackedUmp128& endpointNameMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    switch (internal::GetFormFromStreamMessageFirstWord(endpointNameMessage.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        m_endpointName = ParseStreamTextMessage(endpointNameMessage);
        RETURN_IF_FAILED(UpdateEndpointNameProperty());
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        m_endpointName = ParseStreamTextMessage(endpointNameMessage);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        if (!m_endpointName.empty())
        {
            m_endpointName += ParseStreamTextMessage(endpointNameMessage);
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        if (!m_endpointName.empty())
        {
            m_endpointName += ParseStreamTextMessage(endpointNameMessage);
            RETURN_IF_FAILED(UpdateEndpointNameProperty());
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }

        break;

    default:
        // won't actually happen because the Form field is only 2 bits
        break;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::HandleProductInstanceIdMessage(internal::PackedUmp128& productInstanceIdMessage)
{
    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );

    switch (internal::GetFormFromStreamMessageFirstWord(productInstanceIdMessage.word0))
    {
    case MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE: // complete name in single message. Just update property
        OutputDebugString(L" MIDI_STREAM_MESSAGE_MULTI_FORM_COMPLETE\n" );
        m_productInstanceId = ParseStreamTextMessage(productInstanceIdMessage);
        RETURN_IF_FAILED(UpdateEndpointProductInstanceIdProperty());
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_START: // start of multi-part name message. Overwrite any other name in the map
    {
        OutputDebugString(L" MIDI_STREAM_MESSAGE_MULTI_FORM_START\n");
        m_productInstanceId = ParseStreamTextMessage(productInstanceIdMessage);
    }
    break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE: //continuation of multi-part name message. Append to name in map
        OutputDebugString(L" MIDI_STREAM_MESSAGE_MULTI_FORM_CONTINUE\n");
        if (!m_productInstanceId.empty())
        {
            m_productInstanceId += ParseStreamTextMessage(productInstanceIdMessage);
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }
        break;

    case MIDI_STREAM_MESSAGE_MULTI_FORM_END: // end of multi-part name message. Finish name and update property
        OutputDebugString(L" MIDI_STREAM_MESSAGE_MULTI_FORM_END\n");
        if (!m_productInstanceId.empty())
        {
            m_productInstanceId += ParseStreamTextMessage(productInstanceIdMessage);
            RETURN_IF_FAILED(UpdateEndpointProductInstanceIdProperty());
        }
        else
        {
            // name isn't already started, so a start message was skipped. Don't append anything at all.
        }

        break;

    default:
        // won't actually happen because the Form field is only 2 bits
        break;
    }

    return S_OK;

}
