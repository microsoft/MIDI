// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiBidirectionalEndpointConnection.g.h"

#include "MidiEndpointConnection.h"
#include "InternalMidiDeviceConnection.h"
#include "midi_service_interface.h";
#include "MidiMessagesReceivedEventArgs.h";

#include "MidiUmp32.h";
#include "MidiUmp64.h";
#include "MidiUmp96.h";
#include "MidiUmp128.h";

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, 
        Windows::Devices::Midi2::implementation::MidiEndpointConnection,
        IMidiCallback>
    {
        MidiBidirectionalEndpointConnection() = default;

        static hstring GetDeviceSelectorForBidirectional() { return L""; /* TODO*/ }

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> Filters();
        winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy FilterStrategy();
        void FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value);

        uint32_t ReceiveBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive);

        uint32_t SendBuffer(winrt::Windows::Foundation::IMemoryBuffer const& midiData, uint32_t byteOffset, uint32_t length);
        void SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump);


        bool Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection);

 
        IMPLEMENT_MIDI_MESSAGES_RECEIVED_EVENT

    private:
        com_ptr<IMidiBiDi> _bidiEndpoint;

        bool _tempMessagesReceivedFlag = false;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
