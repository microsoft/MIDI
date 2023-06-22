// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#pragma once
#include "MidiEndpoint.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiEndpoint : MidiEndpointT<MidiEndpoint>
    {
        MidiEndpoint() = default;

        static hstring GetDeviceSelector();
        static hstring GetDeviceSelector(winrt::Microsoft::Devices::Midi2::MidiEndpointNativeDataFormatType const& midiEndpointType);
        hstring DeviceId();
        bool EndpointInformationValid();
        winrt::Microsoft::Devices::Midi2::MidiEndpointInformation EndpointInformation();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Devices::Midi2::MidiFunctionBlock> FunctionBlocks();
        void SendUmp(uint64_t timestamp, winrt::Microsoft::Devices::Midi2::Ump const& ump);
        void SendMultipleUmps(uint64_t timestamp, winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::Ump> const& umps);
        void SendMultipleUmpsAsWords(uint64_t timestamp, winrt::Windows::Foundation::Collections::IVector<uint32_t> const& midiWords);
        winrt::event_token MessagesReceived(winrt::Microsoft::Devices::Midi2::MidiMessagesReceivedEventHandler const& handler);
        void MessagesReceived(winrt::event_token const& token) noexcept;
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiEndpoint : MidiEndpointT<MidiEndpoint, implementation::MidiEndpoint>
    {
    };
}
