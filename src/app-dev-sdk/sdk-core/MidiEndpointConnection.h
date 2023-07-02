// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointConnection.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection>
    {
        MidiEndpointConnection() = default;

        static hstring GetDeviceSelector();
        static hstring GetDeviceSelector(winrt::Microsoft::Devices::Midi2::MidiEndpointDataFormat const& midiEndpointDataFormat);
        hstring DeviceId();
        bool EndpointInformationValid();
        winrt::Microsoft::Devices::Midi2::MidiEndpointInformation EndpointInformation();
        winrt::Windows::Foundation::Collections::IVectorView<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> FunctionBlocks();
        void RequestEndpointInformationAndFunctions(bool forceRefresh);
        winrt::Windows::Foundation::IAsyncAction RequestEndpointInformationAndFunctionsAsync(bool forceRefresh);
        winrt::Microsoft::Devices::Midi2::MidiMessageReader GetMessageReader();
        winrt::Microsoft::Devices::Midi2::MidiMessageWriter GetMessageWriter();
        winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Microsoft::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;


        // TODO: Internal constructor to spin this up with the local inproc loopback rather than an actual device



    private:
        hstring _id;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
