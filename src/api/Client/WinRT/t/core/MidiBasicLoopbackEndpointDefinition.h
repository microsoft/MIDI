#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackEndpointDefinition.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEndpointDefinition : MidiBasicLoopbackEndpointDefinitionT<MidiBasicLoopbackEndpointDefinition>
    {
        MidiBasicLoopbackEndpointDefinition() = default;

        MidiBasicLoopbackEndpointDefinition(_In_ winrt::hstring const& name, _In_ winrt::hstring const& uniqueId);
        MidiBasicLoopbackEndpointDefinition(_In_ winrt::hstring const& name, _In_ winrt::hstring const& uniqueId, _In_ winrt::hstring const& description);

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = value; }

        winrt::hstring UniqueId() const noexcept { return m_uniqueId; }
        void UniqueId(_In_ winrt::hstring const& value) noexcept { m_uniqueId = value; }        

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = value; }

    private:
        winrt::hstring m_name{};
        winrt::hstring m_uniqueId{};
        winrt::hstring m_description{};
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackEndpointDefinition : MidiBasicLoopbackEndpointDefinitionT<MidiBasicLoopbackEndpointDefinition, implementation::MidiBasicLoopbackEndpointDefinition>
    {
    };
}
