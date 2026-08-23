#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackEndpointDefinition.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackEndpointDefinition : MidiBasicLoopbackEndpointDefinitionT<MidiBasicLoopbackEndpointDefinition>
    {
        MidiBasicLoopbackEndpointDefinition() = default;

        MidiBasicLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name)
        {
            Name(name);
        }

        MidiBasicLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description) : MidiBasicLoopbackEndpointDefinition(name)
        {
            Description(description);
        }

        MidiBasicLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name,
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& uniqueId) : MidiBasicLoopbackEndpointDefinition(name, description)
        {
            UniqueId(uniqueId);
        }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TruncateToUtf8ByteCount(value.c_str(), MIDI_MAX_UMP_ENDPOINT_NAME_BYTE_COUNT); }

        winrt::hstring UniqueId() const noexcept { return m_uniqueId; }
        void UniqueId(_In_ winrt::hstring const& value) noexcept { m_uniqueId = internal::TruncateHStringCopy(internal::RemoveInvalidSWDUniqueIdCharacters(value.c_str()).c_str(), MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT); }

        winrt::hstring Description() const noexcept { return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = value; }

        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }
        void ImageFileName(_In_ winrt::hstring const& value) noexcept { m_imageFileName = winrt::hstring{ internal::CleanImageFileName(value.c_str()) }; }

    private:
        winrt::hstring m_name{};
        winrt::hstring m_uniqueId{};
        winrt::hstring m_description{};
        winrt::hstring m_imageFileName{};
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::factory_implementation
{
    struct MidiBasicLoopbackEndpointDefinition : MidiBasicLoopbackEndpointDefinitionT<MidiBasicLoopbackEndpointDefinition, implementation::MidiBasicLoopbackEndpointDefinition>
    {
    };
}
