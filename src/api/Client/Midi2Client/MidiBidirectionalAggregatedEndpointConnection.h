// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#pragma once

#include <pch.h>

#include "MidiBidirectionalAggregatedEndpointConnection.g.h"

#include "midi_service_interface.h"

#include "MidiMessageReceivedEventArgs.h"

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

#include "InternalMidiConnectionCommon.h"
#include "InternalMidiInputConnection.h"
#include "InternalMidiOutputConnection.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiBidirectionalAggregatedEndpointConnection : 
        MidiBidirectionalAggregatedEndpointConnectionT<
            MidiBidirectionalAggregatedEndpointConnection, 
            IMidiCallback>,
        public internal::InternalMidiConnectionCommon,
        public internal::InternalMidiInputConnection<IMidiIn>,
        public internal::InternalMidiOutputConnection<IMidiOut>
    {
        MidiBidirectionalAggregatedEndpointConnection() = default;
        ~MidiBidirectionalAggregatedEndpointConnection();

        winrt::hstring InputDeviceId() const noexcept { return m_inputDeviceId; }
        winrt::hstring OutputDeviceId() const noexcept { return m_outputDeviceId; }

        STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG position) override
        {
            return CallbackImpl(*this, data, size, position);
        }


        _Success_(return == true)
        bool InternalInitialize(
            _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ winrt::hstring const endpointInstanceId,
            _In_ winrt::hstring const deviceIdInputConnection,
            _In_ winrt::hstring const deviceIdOutputConnection
        );

        _Success_(return == true)
        bool Open();


    private:
    };
}
