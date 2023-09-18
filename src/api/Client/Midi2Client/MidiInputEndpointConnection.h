// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>



#include "MidiInputEndpointConnection.g.h"

#include "MidiMessageReceivedEventArgs.h"

#include "midi_service_interface.h"

#include "InternalMidiInputConnection.h"
#include "InternalMidiConnectionCommon.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiInputEndpointConnection : MidiInputEndpointConnectionT<
        MidiInputEndpointConnection, 
        IMidiCallback>,
        public internal::InternalMidiConnectionCommon,
        public internal::InternalMidiInputConnection<IMidiIn>
    {
        MidiInputEndpointConnection() = default;
        ~MidiInputEndpointConnection();

        static winrt::hstring GetDeviceSelector() noexcept { return L"System.Devices.InterfaceClassGuid:=\"{AE174174-6396-4DEE-AC9E-1E9C6F403230}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"; }



        STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG position) override
        {
            return CallbackImpl(*this, data, size, position);
        }

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
    struct MidiInputEndpointConnection : MidiInputEndpointConnectionT<MidiInputEndpointConnection, implementation::MidiInputEndpointConnection>
    {
    };
}
