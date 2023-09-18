// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiPingBidiDevice.h"



HRESULT MidiPingBidiDevice::SendMidiMessage(
    _In_ void* message,
    _In_ UINT32 size,
    _In_ LONGLONG /*timestamp*/)
{
    RETURN_HR_IF_NULL(E_POINTER, m_callback);
    RETURN_HR_IF_NULL(E_INVALIDARG, message);
    RETURN_HR_IF(E_INVALIDARG, size != sizeof(Windows::Devices::Midi2::Internal::PackedPingRequestUmp));
   
    auto requestUmp = (Windows::Devices::Midi2::Internal::PackedPingRequestUmp*)message;

    RETURN_HR_IF(E_INVALIDARG, requestUmp->Word0 != INTERNAL_PING_REQUEST_UMP_WORD0);

    // build response message
    Windows::Devices::Midi2::Internal::PackedPingResponseUmp responseUmp;
    responseUmp.Word0 = INTERNAL_PING_RESPONSE_UMP_WORD0;
    responseUmp.PingId = requestUmp->PingId;

    auto newTimestamp = Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp();

    return m_callback->Callback(&responseUmp, sizeof(Windows::Devices::Midi2::Internal::PackedPingResponseUmp), newTimestamp);
}


void MidiPingBidiDevice::SetCallback(_In_ IMidiCallback* callback)
{
    m_callback = callback;
}

MidiPingBidiDevice::MidiPingBidiDevice() = default;

MidiPingBidiDevice::~MidiPingBidiDevice()
{
    m_callback = nullptr;
}