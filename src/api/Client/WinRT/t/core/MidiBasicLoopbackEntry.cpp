#include "pch.h"
#include "MidiBasicLoopbackEntry.h"
#include "Transports.BasicLoopback.MidiBasicLoopbackEntry.g.cpp"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    _Use_decl_annotations_
    void MidiBasicLoopbackEntry::InternalInitialize(
        winrt::guid const& associationId,
        winrt::hstring const& endpointDeviceId,
        winrt::hstring const& name,
        winrt::hstring const& description,
        winrt::hstring const& imageFileName,
        bool const isMuted) noexcept
    {
        m_associationId = associationId;
        m_endpointDeviceId = endpointDeviceId;
        m_name = name;
        m_description = description;
        m_isMuted = isMuted;

        // the service reports what the configuration file said, and that file is editable by any
        // standard user, so it is cleaned again on the way in
        m_imageFileName = winrt::hstring{ internal::CleanImageFileName(imageFileName.c_str()) };
    }
}
