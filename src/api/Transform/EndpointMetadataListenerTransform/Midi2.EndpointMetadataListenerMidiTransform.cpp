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
    UNREFERENCED_PARAMETER(deviceId);
    UNREFERENCED_PARAMETER(creationParams);
    UNREFERENCED_PARAMETER(mmcssTaskId);
    UNREFERENCED_PARAMETER(MidiDeviceManager);


    OutputDebugString(L"" __FUNCTION__ " Start up");

    TraceLoggingWrite(
        MidiEndpointMetadataListenerTransformTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this")
    );


    m_callback = callback;

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
    // TODO: This really should have a worker thread doing the message processing
    // because we don't want to add any latency or jitter here. Prototype will
    // be all in-line, however.

    RETURN_HR_IF_NULL(E_INVALIDARG, data);

    // Forward immediately. The client receives all messages, even if we handle them here
    if (m_callback)
    {
        m_callback->Callback(data, size, timestamp, m_context);
    }

    // Check to see if type F and if so, send it to be processed

    if (data != nullptr && size == UMP128_BYTE_COUNT)
    {
        internal::PackedUmp128 ump;

        if (internal::FillPackedUmp128FromBytePointer((byte*)data, (uint8_t)size, ump))
        {
            // if type F, process it.

            if (internal::GetUmpMessageTypeFromFirstWord(ump.word0) == 0xF)
            {
                ProcessStreamMessage(data, size, timestamp);
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

    return S_OK;
}

// takes the list of messages making up the name, builds a name string, and then updates
// the device property in the property store. If we end up multi-threading this, it will
// need to lock the list while working on it.
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateEndpointProductInstanceIdProperty()
{

    return S_OK;
}

// The device identity is a single message, so we don't keep a copy. Instead, we just do
// the parsing here and then update the property
_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateDeviceIdentityProperty(internal::PackedUmp128& identityMessage)
{
    UNREFERENCED_PARAMETER(identityMessage);

    return S_OK;
}



HRESULT
CMidi2EndpointMetadataListenerMidiTransform::UpdateFunctionBlocksProperty()
{
    // lock the function blocks
     
    // pull existing property

    // build an updated set of function block headers and strings

    // update the property


    return S_OK;
}


// this function assumes we've already done bounds checking
_Use_decl_annotations_
HRESULT 
CMidi2EndpointMetadataListenerMidiTransform::ProcessStreamMessage(PVOID message, UINT size, LONGLONG timestamp)
{
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(size);
    UNREFERENCED_PARAMETER(timestamp);


    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidi2EndpointMetadataListenerMidiTransform::AddOrUpdateInternalFunctionBlockList(internal::PackedUmp128& relatedMessage)
{
    UNREFERENCED_PARAMETER(relatedMessage);





    return S_OK;
}


