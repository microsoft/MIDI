// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"
#include "MidiEndpointConnection.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiEndpointConnection::GetDeviceSelector()
    {
        throw hresult_not_implemented();
    }
    hstring MidiEndpointConnection::GetDeviceSelector(winrt::Microsoft::Devices::Midi2::MidiEndpointDataFormat const& midiEndpointDataFormat)
    {
        throw hresult_not_implemented();
    }
    hstring MidiEndpointConnection::DeviceId()
    {
        throw hresult_not_implemented();
    }
    bool MidiEndpointConnection::EndpointInformationValid()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpointInformation MidiEndpointConnection::EndpointInformation()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> MidiEndpointConnection::FunctionBlocks()
    {
        throw hresult_not_implemented();
    }
    void MidiEndpointConnection::RequestEndpointInformationAndFunctions(bool forceRefresh)
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::IAsyncAction MidiEndpointConnection::RequestEndpointInformationAndFunctionsAsync(bool forceRefresh)
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiMessageReader MidiEndpointConnection::GetMessageReader()
    {
        throw hresult_not_implemented();
    }
    winrt::Microsoft::Devices::Midi2::MidiMessageWriter MidiEndpointConnection::GetMessageWriter()
    {
        throw hresult_not_implemented();
    }
    winrt::event_token MidiEndpointConnection::MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Microsoft::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler)
    {
        throw hresult_not_implemented();
    }
    void MidiEndpointConnection::MessagesReceived(winrt::event_token const& token) noexcept
    {
        throw hresult_not_implemented();
    }
}