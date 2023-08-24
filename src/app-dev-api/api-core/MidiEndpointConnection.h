// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointConnection.g.h"

#include "midi_service_interface.h"
//#include <functional>

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection>
    {
        MidiEndpointConnection() = default;

        static hstring GetDeviceSelector();

        hstring Id() const noexcept { return m_id; }
        hstring DeviceId() const noexcept { return m_deviceId; }
        bool IsOpen() const noexcept { return m_isOpen; }
        IMidiEndpointDefinedConnectionSettings Settings() noexcept { return m_settings; }

        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharing ActiveSharingMode() { return m_activeSharingMode; }

        IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ IInspectable value) noexcept { m_tag = value; }


        void InternalSetSettings(_In_ IMidiEndpointDefinedConnectionSettings value) { m_settings = value; }
        void InternalSetDeviceId(_In_ hstring value) { m_deviceId = value; }
        void InternalSetId(_In_ hstring value) { m_id = value; }
        void InternalSetIsConnected(_In_ bool value) { m_isOpen = value; }

    protected:
        hstring m_id {};
        hstring m_deviceId {};
        IInspectable m_tag{ nullptr };
        winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharing m_activeSharingMode{ winrt::Windows::Devices::Midi2::MidiEndpointConnectionSharing::Unknown };
        
        bool m_isOpen{ false };

        IMidiEndpointDefinedConnectionSettings m_settings{ nullptr };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointConnection : MidiEndpointConnectionT<MidiEndpointConnection, implementation::MidiEndpointConnection>
    {
    };
}
