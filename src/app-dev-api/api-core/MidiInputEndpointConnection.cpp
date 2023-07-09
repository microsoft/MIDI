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
    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> MidiInputEndpointConnection::Filters()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy MidiInputEndpointConnection::FilterStrategy()
    {
        throw hresult_not_implemented();
    }
    void MidiInputEndpointConnection::FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value)
    {
        throw hresult_not_implemented();
    }
    uint32_t MidiInputEndpointConnection::ReceiveBuffer(winrt::Windows::Devices::Midi2::MidiMessageBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive)
    {
        throw hresult_not_implemented();
    }



    //IFACEMETHODIMP MidiInputEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    //{
    //    // TODO: process incoming messages / fire event

    //    // check each reader. For each reader that returns True when queried, fire off the messagesreceived event


    //    return S_OK;
    //}

    bool MidiInputEndpointConnection::Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection)
    {
        return false;
    }

}
