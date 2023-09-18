// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

#include "MidiOutputEndpointConnection.g.h"
#include "midi_service_interface.h"

#include "InternalMidiOutputConnection.h"
#include "InternalMidiConnectionCommon.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<
        MidiOutputEndpointConnection>,
        public internal::InternalMidiConnectionCommon,
        public internal::InternalMidiOutputConnection<IMidiOut>
    {
        MidiOutputEndpointConnection() = default;
        ~MidiOutputEndpointConnection();

        static winrt::hstring GetDeviceSelector() noexcept { return L"System.Devices.InterfaceClassGuid:=\"{3705DC2B-17A7-4452-98CE-BF12C6F48A0B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"; }


        _Success_(return == true)
        bool InternalInitialize(
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ winrt::guid const connectionId,
            _In_ winrt::hstring const endpointDeviceId);

        _Success_(return == true)
            bool Open();

        // IClosable
        void Close();

    private:
        bool m_closeHasBeenCalled{ false };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiOutputEndpointConnection : MidiOutputEndpointConnectionT<MidiOutputEndpointConnection, implementation::MidiOutputEndpointConnection>
    {
    };
}
