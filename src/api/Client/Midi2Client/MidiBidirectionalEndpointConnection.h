// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>


#include "MidiBidirectionalEndpointConnection.g.h"

#include "midi_service_interface.h"

#include "MidiMessageReceivedEventArgs.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

#include "InternalMidiConnectionCommon.h"
#include "InternalMidiInputConnection.h"
#include "InternalMidiOutputConnection.h"
#include "InternalMidiFunctionBlockDevice.h"
#include "InternalMidiEndpointInformationDevice.h"




namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<
            MidiBidirectionalEndpointConnection, 
            IMidiCallback>, 
        public internal::InternalMidiConnectionCommon,
        public internal::InternalMidiInputConnection<IMidiBiDi>,
        public internal::InternalMidiOutputConnection<IMidiBiDi>,
        public internal::InternalMidiFunctionBlockDevice,
        public internal::InternalMidiEndpointInformationDevice
    {
        MidiBidirectionalEndpointConnection() = default;
        ~MidiBidirectionalEndpointConnection();

        static hstring GetDeviceSelector() noexcept { return L"System.Devices.InterfaceClassGuid:=\"{E7CCE071-3C03-423f-88D3-F1045D02552B}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"; }

        //winrt::hstring InputEndpointDeviceId() const noexcept { return InternalGetDeviceId(); }
        //winrt::hstring OutputEndpointDeviceId() const noexcept { return InternalGetDeviceId(); } // will be the same as input

        STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG position) override
        {
            return CallbackImpl(*this, data, size, position);
        }

        _Success_(return == true)
        bool InternalInitialize(
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ winrt::hstring const connectionId,
            _In_ winrt::hstring const endpointDeviceId,
            _In_ midi2::MidiBidirectionalEndpointOpenOptions options);

        _Success_(return == true)
        bool Open();

    private:
        // We use the InputDeviceId. InputDeviceId and OutputDeviceId 
        // are the same thing. It's a bit hokey, but many other parts
        // of the API and plugins need to have the device IDs defined at
        // the interface (IMidiInputConnection, IMIdiOutputConnection) 
        // instead of at the class level.
        winrt::hstring InternalGetDeviceId() const noexcept { return m_inputDeviceId; }



    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiBidirectionalEndpointConnection : MidiBidirectionalEndpointConnectionT<MidiBidirectionalEndpointConnection, implementation::MidiBidirectionalEndpointConnection>
    {
    };
}
