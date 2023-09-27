// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointInformationConfigurator.g.h"

#include "string_util.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointInformationConfigurator : MidiEndpointInformationConfiguratorT<MidiEndpointInformationConfigurator>
    {
        MidiEndpointInformationConfigurator() = default;

        hstring Id() const noexcept { return m_id; }
        void Id(_In_ hstring const& value) noexcept { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        hstring Name() const noexcept { return m_name; }
        void Name(_In_ hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }

        winrt::Windows::Foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ winrt::Windows::Foundation::IInspectable const& value) { m_tag = value; }

        midi2::IMidiInputConnection InputConnection() const noexcept { return m_inputConnection; }
        void InputConnection(_In_ midi2::IMidiInputConnection const& value) noexcept { m_inputConnection = value; }

        void ProcessIncomingMessage(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent);


        void Initialize();
        void OnEndpointConnectionOpened();
        void Cleanup();


    private:
        winrt::hstring m_id{};
        winrt::hstring m_name{};
        bool m_enabled{ false };
        foundation::IInspectable m_tag{ nullptr };
        midi2::IMidiInputConnection m_inputConnection;
    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiEndpointInformationConfigurator : MidiEndpointInformationConfiguratorT<MidiEndpointInformationConfigurator, implementation::MidiEndpointInformationConfigurator>
    {
    };
}
