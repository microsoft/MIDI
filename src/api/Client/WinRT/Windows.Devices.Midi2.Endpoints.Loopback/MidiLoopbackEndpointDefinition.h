// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "MidiLoopbackEndpointDefinition.g.h"

namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::implementation
{
    struct MidiLoopbackEndpointDefinition : MidiLoopbackEndpointDefinitionT<MidiLoopbackEndpointDefinition>
    {
        MidiLoopbackEndpointDefinition() = default;

        MidiLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& uniqueId)
        {
            Name(name);
            UniqueId(uniqueId);
        }

        MidiLoopbackEndpointDefinition(
            _In_ winrt::hstring const& name, 
            _In_ winrt::hstring const& uniqueId, 
            _In_ winrt::hstring const& description)
        {
            Name(name);
            UniqueId(uniqueId);
            Description(description);
        }

        winrt::hstring Name() const noexcept { return m_name; }
        void Name(_In_ winrt::hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        winrt::hstring UniqueId() const noexcept { return m_uniqueId; }
        void UniqueId(_In_ winrt::hstring const& value) noexcept { m_uniqueId = internal::TrimmedHStringCopy(value); }

        winrt::hstring Description() const noexcept{ return m_description; }
        void Description(_In_ winrt::hstring const& value) noexcept { m_description = internal::TrimmedHStringCopy(value); }

    private:
        winrt::hstring m_description{ };
        winrt::hstring m_name{ };
        winrt::hstring m_uniqueId{ };
    };
}
namespace winrt::Windows::Devices::Midi2::Endpoints::Loopback::factory_implementation
{
    struct MidiLoopbackEndpointDefinition : MidiLoopbackEndpointDefinitionT<MidiLoopbackEndpointDefinition, implementation::MidiLoopbackEndpointDefinition>
    {
    };
}
