// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "midi2.VirtualMidiabstraction.h"

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::Initialize(
    LPCWSTR endpointId,
    PABSTRACTIONCREATIONPARAMS,
    DWORD *,
    IMidiCallback * Callback,
    LONGLONG Context
)
{
    OutputDebugString(__FUNCTION__ L" - enter\n");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__, 
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointId, "endpoint id")
        );

    m_callback = Callback;
    m_callbackContext = Context;
    m_endpointId = internal::NormalizeEndpointInterfaceIdCopy(endpointId);
  
    //if (Context != MIDI_PROTOCOL_MANAGER_ENDPOINT_CREATION_CONTEXT)
    {
        OutputDebugString(__FUNCTION__ L" Looking up Endpoint:");
        OutputDebugString(m_endpointId.c_str());

        HRESULT hr = S_OK;

        // This should use SWD properties and not a string search

        if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_VIRT_INSTANCE_ID_DEVICE_PREFIX))
        {
            OutputDebugString(__FUNCTION__ L" - endpoint id is a virtual device\n");

            m_isDeviceSide = true;

            LOG_IF_FAILED(hr = AbstractionState::Current().GetEndpointTable()->OnDeviceConnected(m_endpointId, this));
        }
        else if (internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_VIRT_INSTANCE_ID_CLIENT_PREFIX))
        {
            OutputDebugString(__FUNCTION__ L" - endpoint id is a virtual client\n");

            m_isDeviceSide = false;

            LOG_IF_FAILED(hr = AbstractionState::Current().GetEndpointTable()->OnClientConnected(m_endpointId, this));
        }
        else
        {
            OutputDebugString(__FUNCTION__ L" - endpoint id is unknown type\n");
            OutputDebugString(m_endpointId.c_str());

            // we don't understand this endpoint id

            hr = E_FAIL;
        }

        return hr;
    }
    //else
    //{
    //    // we're in protocol negotiation
    //    m_isDeviceSide = false;
    //    return S_OK;
    //}
}

HRESULT
CMidi2VirtualMidiBiDi::Cleanup()
{
    OutputDebugString(__FUNCTION__ L" - enter\n");

    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
        TraceLoggingBool(m_isDeviceSide, "is device side")
        );

    m_callback = nullptr;
    m_callbackContext = 0;

    if (m_isDeviceSide)
    {
        OutputDebugString(__FUNCTION__ L" - this is the device BiDi, so calling OnDeviceDisconnected\n");

        LOG_IF_FAILED(AbstractionState::Current().GetEndpointTable()->OnDeviceDisconnected(m_endpointId));
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" - this is the client BiDi. Nothing needed here.\n");
    }

    UnlinkAssociatedBiDi();

    OutputDebugString(__FUNCTION__ L" - exit\n");

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::SendMidiMessage(
    PVOID Message,
    UINT Size,
    LONGLONG Position
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
        TraceLoggingBool(m_isDeviceSide, "is device side")
    );

    // message received from the device

    RETURN_HR_IF_NULL(E_INVALIDARG, Message);
    RETURN_HR_IF(E_INVALIDARG, Size < sizeof(uint32_t));

    // if there's no linked bidi, it's not a failure. We just lose the message
    if (m_linkedBiDiCallback != nullptr)
    {
        //return m_linkedBiDi->SendMidiMessage(Message, Size, Position);
        return m_linkedBiDiCallback->Callback(Message, Size, Position, m_callbackContext);
    }

    return S_OK;

}

_Use_decl_annotations_
HRESULT
CMidi2VirtualMidiBiDi::Callback(
    PVOID Message,
    UINT Size,
    LONGLONG Position,
    LONGLONG Context
)
{
    TraceLoggingWrite(
        MidiVirtualMidiAbstractionTelemetryProvider::Provider(),
        __FUNCTION__,
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id"),
        TraceLoggingBool(m_isDeviceSide, "is device side")
    );

    // message received from the client

    if (m_callback != nullptr)
    {
        return m_callback->Callback(Message, Size, Position, Context);
    }

    return S_OK;
}

