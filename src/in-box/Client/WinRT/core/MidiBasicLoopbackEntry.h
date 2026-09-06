#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackEntry.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEntry : MidiBasicLoopbackEntryT<MidiBasicLoopbackEntry>
    {
        MidiBasicLoopbackEntry() = default;

        winrt::guid AssociationId() const noexcept { return m_associationId; }
        winrt::hstring EndpointDeviceId() const noexcept { return m_endpointDeviceId; }
        winrt::hstring Name() const noexcept { return m_name; }
        winrt::hstring Description() const noexcept { return m_description; }
        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }
        bool IsMuted() const noexcept { return m_isMuted; }
        uint64_t MessageCount() const noexcept { return m_messageCount; }

        void InternalInitialize(
            _In_ winrt::guid const& associationId, 
            _In_ winrt::hstring const& endpointDeviceId, 
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description, 
            _In_ winrt::hstring const& imageFileName,
            _In_ bool const isMuted,
            _In_ uint64_t const messageCount) noexcept;

    private:
        winrt::guid m_associationId{};
        winrt::hstring m_endpointDeviceId{};
        winrt::hstring m_name{};
        winrt::hstring m_description{};
        winrt::hstring m_imageFileName{};
        bool m_isMuted{ false };
        uint64_t m_messageCount{ 0 };

    };
}
