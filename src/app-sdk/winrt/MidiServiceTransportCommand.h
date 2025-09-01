// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "ServiceConfig.MidiServiceTransportCommand.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceTransportCommand : MidiServiceTransportCommandT<MidiServiceTransportCommand>
    {
        MidiServiceTransportCommand() = default;

        MidiServiceTransportCommand(
            _In_ winrt::guid const& transportId) noexcept;

        MidiServiceTransportCommand(
            _In_ winrt::guid const& transportId, 
            _In_ winrt::hstring const& verb) noexcept;

        MidiServiceTransportCommand(
            _In_ winrt::guid const& transportId, 
            _In_ winrt::hstring const& verb, 
            collections::IMap<winrt::hstring, winrt::hstring> const& arguments) noexcept;

        winrt::hstring Verb() const noexcept { return m_verb; }
        void Verb(_In_ winrt::hstring const& value) noexcept { m_verb = internal::TrimmedHStringCopy(value); }

        collections::IMap<winrt::hstring, winrt::hstring> Arguments() noexcept { return m_arguments; }
        void Arguments(_In_ collections::IMap<winrt::hstring, winrt::hstring> const& value) noexcept { m_arguments = value; }

        winrt::guid TransportId() const noexcept { return m_transportId; }
        void TransportId(_In_ winrt::guid const& value) noexcept { m_transportId = value; }

        json::JsonObject GetConfigJson() const noexcept;

    private:
        winrt::hstring m_verb{};
        winrt::guid m_transportId{};

        collections::IMap<winrt::hstring, winrt::hstring> m_arguments = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::ServiceConfig::factory_implementation
{
    struct MidiServiceTransportCommand : MidiServiceTransportCommandT<MidiServiceTransportCommand, implementation::MidiServiceTransportCommand>
    {
    };
}
