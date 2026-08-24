// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Loopback.MidiLoopbackEndpointDefinition.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Loopback::implementation
{
    struct MidiLoopbackEndpointDefinition : MidiLoopbackEndpointDefinitionT<MidiLoopbackEndpointDefinition>
    {
        MidiLoopbackEndpointDefinition() = default;

        MidiLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name)
        {
            Name(name);
        }

        MidiLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& description) : MidiLoopbackEndpointDefinition(name)
        {
            Description(description);
        }

        MidiLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name,  
            _In_ winrt::hstring const& description,
            _In_ winrt::hstring const& uniqueId) : MidiLoopbackEndpointDefinition(name, description)
        {
            UniqueId(uniqueId);
        }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        winrt::hstring UniqueId() const noexcept { return m_uniqueId; }
        void UniqueId(_In_ winrt::hstring const& value) noexcept { m_uniqueId = internal::TruncateHStringCopy(internal::RemoveInvalidSWDUniqueIdCharacters(value.c_str()).c_str(), MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT); }

        winrt::hstring Description() const noexcept{ return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = internal::TrimmedHStringCopy(value); }

        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }
        void ImageFileName(_In_ winrt::hstring const& value) noexcept { m_imageFileName = winrt::hstring{ internal::CleanImageFileName(value.c_str()) }; }

        bool CreateOnlyUmpEndpoint() const noexcept { return m_umpOnly; }
        void CreateOnlyUmpEndpoint(_In_ bool const value) noexcept { m_umpOnly = value; }

    private:
        winrt::hstring m_description{ };
        winrt::hstring m_name{ };
        winrt::hstring m_uniqueId{ };
        winrt::hstring m_imageFileName{ };
        bool m_umpOnly{ false };
    };
}
namespace winrt::Windows::Devices::Midi2::Transports::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointDefinition : MidiLoopbackEndpointDefinitionT<MidiLoopbackEndpointDefinition, implementation::MidiLoopbackEndpointDefinition>
    {
    };
}
