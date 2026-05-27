#pragma once
#include "MidiBasicLoopbackEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEntry : MidiBasicLoopbackEntryT<MidiBasicLoopbackEntry>
    {
        MidiBasicLoopbackEntry() = default;

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Description() const noexcept { return m_description; }
        bool IsMuted() const noexcept { return m_isMuted; }

        collections::IVector<midi2enum::MidiEndpointAssociatedPortDeviceInformation> AssociatedLegacyPorts() const noexcept { return m_associatedLegacyPorts; }

        void InternalInitialize(
            _In_ winrt::guid const& associationId, 
            _In_ winrt::hstring const& endpointDeviceId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description, 
            _In_ bool const isMuted, 
            _In_ collections::IVector<midi2enum::MidiEndpointAssociatedPortDeviceInformation> associatedLegacyPorts) noexcept;

    private:
        collections::IVector<midi2enum::MidiEndpointAssociatedPortDeviceInformation> m_associatedLegacyPorts { nullptr };

        winrt::guid m_associationId{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_name{};
        winrt::hstring m_description{};
        bool m_isMuted{ false };

    };
}
