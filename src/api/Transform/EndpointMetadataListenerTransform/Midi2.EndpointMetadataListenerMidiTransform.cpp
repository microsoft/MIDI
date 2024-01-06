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

// The device identity is a single message, so we don't keep a copy. Instead, we just do
// the parsing here and then update the property. We assume this has already been identified
// as a type=F, form=0, status=0x02 stream message
_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateDeviceIdentityProperty(internal::PackedUmp128& identityMessage)
{
    OutputDebugString(L"" __FUNCTION__);

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


    DEVPROPERTY props[] = {
    {
        {PKEY_MIDI_DeviceIdentity, DEVPROP_STORE_SYSTEM, nullptr},
            DEVPROP_TYPE_BINARY, static_cast<ULONG>(sizeof(prop)), &prop},
    };

    RETURN_IF_FAILED(m_MidiDeviceManager->UpdateEndpointProperties(m_deviceInstanceId.c_str(), ARRAYSIZE(props), (PVOID)props));

    OutputDebugString(L"" __FUNCTION__ " Property Updated");


    return S_OK;
}



HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateFunctionBlocksProperty()
{
    OutputDebugString(L"" __FUNCTION__);

    // lock the function blocks
     
    // pull existing property

    // build an updated set of function block headers and strings

    // update the property


    return S_OK;
}

#define MIDI_DEVICE_IDENTITY_STREAM_MESSAGE_FORM 0
#define MIDI_DEVICE_IDENTITY_STREAM_MESSAGE_STATUS 0x02


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
        // single message contains all we need, so super simple. Just update
        UpdateDeviceIdentityProperty(ump);
    }
    else
    {
        // TODO other message types
    }




    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::AddInternalFunctionBlockNameMessage(internal::PackedUmp128& functionBlockNameMessage)
{
    UNREFERENCED_PARAMETER(functionBlockNameMessage);





    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::AddOrUpdateInternalFunctionBlockList(internal::PackedUmp128& functionBlockInfoMessage)
{
    UNREFERENCED_PARAMETER(functionBlockInfoMessage);





    return S_OK;
}


