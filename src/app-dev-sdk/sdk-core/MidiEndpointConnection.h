// Copyright (c) Microsoft Corporation.
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
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> FunctionBlocks();
        winrt::event_token MessagesReceived(winrt::Windows::Foundation::EventHandler<winrt::Microsoft::Devices::Midi2::MidiMessagesReceivedEventArgs> const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;
        void RequestEndpointInformationAndFunctions(bool forceRefresh);
        winrt::Windows::Foundation::IAsyncAction RequestEndpointInformationAndFunctionsAsync(bool forceRefresh);
        void SendUmp(winrt::Microsoft::Devices::Midi2::UmpWithTimestamp const& ump);
        void SendMultipleUmps(winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::UmpWithTimestamp> const& umps);
        void SendMultipleUmpsAsWords(uint64_t timestamp, winrt::Windows::Foundation::Collections::IVector<uint32_t> const& midiWords);
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
