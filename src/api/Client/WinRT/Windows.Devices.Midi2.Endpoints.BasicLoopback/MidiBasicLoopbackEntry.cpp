#include "pch.h"
#include "MidiBasicLoopbackEntry.h"
#include "MidiBasicLoopbackEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    _Use_decl_annotations_
    void MidiBasicLoopbackEntry::InternalInitialize(
        winrt::guid const& associationId,
        winrt::hstring const& endpointDeviceId,
        winrt::hstring const& name,
        winrt::hstring const& description,
        bool const isMuted) noexcept
    {
        m_associationId = associationId;
        m_endpointDeviceId = endpointDeviceId;
        m_name = name;
        m_description = description;
        m_isMuted = isMuted;

    }
}
