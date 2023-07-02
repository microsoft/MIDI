// Copyright(c) Microsoft Corporation.
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
    // internal constructor
    MidiEndpointConnection::MidiEndpointConnection(winrt::hstring id)
    {
        _id = id;
    }



    hstring MidiEndpointConnection::GetDeviceSelector()
    {
        // TODO
        return L"";
    }
    hstring MidiEndpointConnection::GetDeviceSelector(winrt::Microsoft::Devices::Midi2::MidiEndpointDataFormat const& midiEndpointDataFormat)
    {
        // TODO
        return L"";
    }
    hstring MidiEndpointConnection::DeviceId()
    {
        return _id;
    }
    bool MidiEndpointConnection::EndpointInformationValid()
    {
        return false;
    }
    winrt::Microsoft::Devices::Midi2::MidiEndpointInformation MidiEndpointConnection::EndpointInformation()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> MidiEndpointConnection::FunctionBlocks()
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
