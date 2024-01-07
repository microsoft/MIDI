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
    OutputDebugString(L"" __FUNCTION__);

    DEVPROPKEY key;
    GUID propertyId;

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
    OutputDebugString(L"" __FUNCTION__);

    DEVPROPKEY key;
    GUID propertyId;

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


    OutputDebugString(L"" __FUNCTION__ " Start up, Provided device id:");
    OutputDebugString(deviceId);

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

    try
    {
        OutputDebugString(L"" __FUNCTION__ " Shut down time");

        return S_OK;
    }
    catch (...)
    {
        OutputDebugString(L"" __FUNCTION__ " Exception cleaning up");

        // TODO: Log
        return S_OK;    // we don't care when cleaning up
    }
}





_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::SendMidiMessage(
    PVOID data,
    UINT size,
    LONGLONG timestamp)
{
 //   OutputDebugString(L"" __FUNCTION__);

    // TODO: This really should have a worker thread doing the message processing
    // because we don't want to add any latency or jitter here. Prototype will
    // be all in-line, however.

    RETURN_HR_IF_NULL(E_INVALIDARG, data);

    // Forward immediately. The client receives all messages, even if we handle them here
    if (m_callback)
    {
        m_callback->Callback(data, size, timestamp, m_context);
    }

    if (data != nullptr && size == UMP128_BYTE_COUNT)
    {
        internal::PackedUmp128 ump;

        if (internal::FillPackedUmp128FromBytePointer((byte*)data, (uint8_t)size, ump))
        {
            // if type F, process it.

            if (internal::GetUmpMessageTypeFromFirstWord(ump.word0) == 0xF)
            {
                ProcessStreamMessage(ump, timestamp);
            }
            else
            {
                // not a stream message. Ignore and move on
            }
        }
        else
        {
            // couldn't fill the UMP. Shouldn't happen since we pre-validate
            return E_FAIL;
        }
    }
    else
    {
        // Either null (hopefully not) or not a UMP128 so can't be a stream message. Fall out quickly
    }

    return S_OK;
}



// takes the list of messages making up the name, builds a name string, and then updates
// the device property in the property store. If we end up multi-threading this, it will
// need to lock the list while working on it.
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateEndpointNameProperty()
{
    OutputDebugString(L"" __FUNCTION__);

    return S_OK;
}

// takes the list of messages making up the name, builds a name string, and then updates
// the device property in the property store. If we end up multi-threading this, it will
// need to lock the list while working on it.
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateEndpointProductInstanceIdProperty()
{
    OutputDebugString(L"" __FUNCTION__);

    return S_OK;
}

_Use_decl_annotations_
HRESULT 
CMidi2EndpointMetadataListenerMidiTransform::UpdateStreamConfigurationProperties(internal::PackedUmp128& endpointStreamConfigurationNotificationMessage)
{
    //OutputDebugString(L"" __FUNCTION__);

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
    //OutputDebugString(L"" __FUNCTION__);

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
    //OutputDebugString(L"" __FUNCTION__);

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


HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateFunctionBlockProperty(internal::PackedUmp128& functionBlockInfoNotificationMessage)
{
    OutputDebugString(L"" __FUNCTION__);

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

// these should be stored in a common file. Maybe the ump*.h ones

#define MIDI_ENDPOINT_INFO_NOTIFICATION_STREAM_MESSAGE_FORM 0
#define MIDI_ENDPOINT_INFO_NOTIFICATION_STREAM_MESSAGE_STATUS 0x01

#define MIDI_DEVICE_IDENTITY_STREAM_MESSAGE_FORM 0
#define MIDI_DEVICE_IDENTITY_STREAM_MESSAGE_STATUS 0x02

#define MIDI_STREAM_CONFIGURATION_STREAM_MESSAGE_FORM 0
#define MIDI_STREAM_CONFIGURATION_STREAM_MESSAGE_STATUS 0x06

#define MIDI_FUNCTION_BLOCK_INFO_NOTIFICATION_STREAM_MESSAGE_FORM 0
#define MIDI_FUNCTION_BLOCK_INFO_NOTIFICATION_STREAM_MESSAGE_STATUS 0x11

// this function assumes we've already done bounds checking
_Use_decl_annotations_
HRESULT 
CMidi2EndpointMetadataListenerMidiTransform::ProcessStreamMessage(internal::PackedUmp128 ump, LONGLONG timestamp)
{
    OutputDebugString(L"" __FUNCTION__);

    UNREFERENCED_PARAMETER(timestamp);
    
    if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_DEVICE_IDENTITY_STREAM_MESSAGE_FORM &&
        internal::GetStatusFromStreamMessageFirstWord(ump.word0) == MIDI_DEVICE_IDENTITY_STREAM_MESSAGE_STATUS)
    {
        OutputDebugString(__FUNCTION__ L" Message is Device Identity\n");
        // single message contains all we need, so super simple. Just update
        UpdateDeviceIdentityProperty(ump);
    }
    else if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_ENDPOINT_INFO_NOTIFICATION_STREAM_MESSAGE_FORM &&
        internal::GetStatusFromStreamMessageFirstWord(ump.word0) == MIDI_ENDPOINT_INFO_NOTIFICATION_STREAM_MESSAGE_STATUS)
    {
        OutputDebugString(__FUNCTION__ L" Message is Endpoint Info\n");
        UpdateEndpointInfoProperties(ump);
    }
    else if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_CONFIGURATION_STREAM_MESSAGE_FORM &&
        internal::GetStatusFromStreamMessageFirstWord(ump.word0) == MIDI_STREAM_CONFIGURATION_STREAM_MESSAGE_STATUS)
    {
        OutputDebugString(__FUNCTION__ L" Message is Stream Configuration\n");
        UpdateStreamConfigurationProperties(ump);
    }
    else if (internal::GetFormFromStreamMessageFirstWord(ump.word0) == MIDI_FUNCTION_BLOCK_INFO_NOTIFICATION_STREAM_MESSAGE_FORM &&
        internal::GetStatusFromStreamMessageFirstWord(ump.word0) == MIDI_FUNCTION_BLOCK_INFO_NOTIFICATION_STREAM_MESSAGE_STATUS)
    {
        OutputDebugString(__FUNCTION__ L" Message is Function Block Info\n");

        UpdateFunctionBlockProperty(ump);
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" Message is unidentified stream message\n");
    }

    // TODO: Other message types


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::AddInternalFunctionBlockNameMessage(internal::PackedUmp128& functionBlockNameMessage)
{
    UNREFERENCED_PARAMETER(functionBlockNameMessage);





    return S_OK;
}


