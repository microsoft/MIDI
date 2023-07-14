// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiInputEndpointConnection.h"
#include "MidiInputEndpointConnection.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    IFACEMETHODIMP MidiInputEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    {
        return S_OK;
    }



    uint32_t MidiInputEndpointConnection::ReceiveBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive)
    {
        throw hresult_not_implemented();
    }


    bool MidiInputEndpointConnection::InternalStart(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection)
    {
        return false;
    }




}
