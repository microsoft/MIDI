// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
HRESULT
CMidi2MidiSynthBidi::Initialize(
    LPCWSTR endpointId,
    PTRANSPORTCREATIONPARAMS,
    DWORD*,
    IMidiCallback* callback,
    LONGLONG context,
    GUID /* sessionId */
)
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(endpointId, "endpoint id")
    );

    RETURN_HR_IF_NULL(E_INVALIDARG, callback);
    RETURN_HR_IF_NULL(E_INVALIDARG, endpointId);

    m_endpointId = internal::NormalizeEndpointInterfaceIdWStringCopy(endpointId);

    RETURN_HR_IF(E_INVALIDARG, m_endpointId.empty());

    if (!internal::EndpointInterfaceIdContainsString(m_endpointId, MIDI_SYNTH_INSTANCE_ID_PREFIX))
    {
        TraceLoggingWrite(
            MidiSynthTransportTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingPointer(this, "this"),
            TraceLoggingWideString(L"We don't understand the endpoint Id", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
        );

        return E_INVALIDARG;
    }

    auto device = TransportState::Current().GetDevice();
    RETURN_HR_IF_NULL(E_UNEXPECTED, device);

    RETURN_IF_FAILED(device->ConnectClient(callback, context));

    {
        auto lock = m_deviceLock.lock_exclusive();
        m_device = std::move(device);
    }

    return S_OK;
}


HRESULT
CMidi2MidiSynthBidi::Shutdown()
{
    TraceLoggingWrite(
        MidiSynthTransportTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingWideString(m_endpointId.c_str(), "endpoint id")
    );

    std::shared_ptr<MidiSynthDevice> device;
    {
        auto lock = m_deviceLock.lock_exclusive();
        device = std::move(m_device);
        m_device.reset();
    }

    if (device)
    {
        // The device itself is owned by the transport state and must survive so the endpoint can
        // be opened again. Only this connection ends here, which releases the audio device.
        LOG_IF_FAILED(device->DisconnectClient());
    }

    return S_OK;
}


#pragma push_macro("SendMessage")
#undef SendMessage
_Use_decl_annotations_
HRESULT
CMidi2MidiSynthBidi::SendMidiMessage(
    MessageOptionFlags optionFlags,
    PVOID message,
    UINT size,
    LONGLONG position
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

    // Snapshot under the lock so a concurrent Shutdown cannot reset it between the null check and
    // the call, and release the lock before calling so it is never held across the send.
    std::shared_ptr<MidiSynthDevice> device;
    {
        auto lock = m_deviceLock.lock_shared();
        device = m_device;
    }

    RETURN_HR_IF_NULL(E_UNEXPECTED, device);

    RETURN_IF_FAILED(device->SendMessage(optionFlags, message, size, position));

    return S_OK;
}
#pragma pop_macro("SendMessage")
