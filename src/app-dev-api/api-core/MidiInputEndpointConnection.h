// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiInputEndpointConnection.g.h"
#include "MidiEndpointConnection.h"

#include "InternalMidiDeviceConnection.h"
#include "midi_service_interface.h";


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiInputEndpointConnection : MidiInputEndpointConnectionT<MidiInputEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection,
        IMidiCallback>
    {
        MidiInputEndpointConnection() = default;

        static hstring GetDeviceSelectorForInput() { return L""; /* TODO */ }

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> Filters();
        winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy FilterStrategy();
        void FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value);
        uint32_t ReceiveBuffer(winrt::Windows::Devices::Midi2::MidiMessageBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive);

        bool Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection);

        IMPLEMENT_MIDI_MESSAGES_RECEIVED_EVENT

    private:




    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiInputEndpointConnection : MidiInputEndpointConnectionT<MidiInputEndpointConnection, implementation::MidiInputEndpointConnection>
    {
    };
}
