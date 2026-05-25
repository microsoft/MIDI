#include "pch.h"
#include "MidiBasicLoopbackEndpointDefinition.h"
#include "MidiBasicLoopbackEndpointDefinition.g.cpp"

namespace winrt::Windows::Devices::Midi2::Endpoints::BasicLoopback::implementation
{
    _Use_decl_annotations_
    MidiBasicLoopbackEndpointDefinition::MidiBasicLoopbackEndpointDefinition(
        winrt::hstring const& name, 
        winrt::hstring const& uniqueId)
    {
        m_name = name;
        m_uniqueId = uniqueId;
    }

    _Use_decl_annotations_
    MidiBasicLoopbackEndpointDefinition::MidiBasicLoopbackEndpointDefinition(
        winrt::hstring const& name, 
        winrt::hstring const& uniqueId, 
        winrt::hstring const& description)
    {
        m_name = name;
        m_uniqueId = uniqueId;
        m_description = description;
    }

}
