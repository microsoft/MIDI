// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

#include "MidiEndpointInformationUpdatedEventArgs.h"
#include "MidiDeviceMetadataCache.h"
#include "MidiEndpointInformation.h"

namespace Windows::Devices::Midi2::Internal
{
    class InternalMidiEndpointInformationDevice
    {
    public:
        void RequestUpdatedEndpointInformation()
        {
            // TODO
        }

        bool IsEndpointInformationValid() { return m_isEndpointInformationValid; }

        midi2::MidiEndpointInformation EndpointInformation() { return m_endpointInformation; }

        winrt::event_token EndpointInformationUpdated(
            _In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiEndpointInformationUpdatedEventArgs> const& handler)
        {
            return m_informationUpdatedEvent.add(handler);
        }

        void EndpointInformationUpdated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_informationUpdatedEvent) m_informationUpdatedEvent.remove(token);
        }

    private:
        bool m_isEndpointInformationValid{ false };

        midi2::MidiEndpointInformation m_endpointInformation{};

        winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiEndpointInformationUpdatedEventArgs>> m_informationUpdatedEvent;


    };
}