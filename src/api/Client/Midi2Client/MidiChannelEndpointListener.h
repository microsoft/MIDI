// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiChannelEndpointListener.g.h"

#include "string_util.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiChannelEndpointListener : MidiChannelEndpointListenerT<MidiChannelEndpointListener>
    {
        MidiChannelEndpointListener() = default;

        hstring Id() const noexcept { return m_id; }
        void Id(_In_ hstring const& value) noexcept { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        hstring Name() const noexcept { return m_name; }
        void Name(_In_ hstring const& value) noexcept { m_name = internal::TrimmedHStringCopy(value); }

        bool IsEnabled() const noexcept { return m_enabled; }
        void IsEnabled(_In_ bool const& value) noexcept { m_enabled = value; }


        winrt::Windows::Foundation::IInspectable Tag() const noexcept { return m_tag; }
        void Tag(_In_ winrt::Windows::Foundation::IInspectable const& value) { m_tag = value; }

        winrt::Windows::Devices::Midi2::IMidiInputConnection InputConnection() const noexcept { return m_inputConnection; }
        void InputConnection(_In_ winrt::Windows::Devices::Midi2::IMidiInputConnection const& value) noexcept { m_inputConnection = value; }


        winrt::Windows::Devices::Midi2::MidiGroup IncludeGroup() const noexcept { return m_includedGroup; }
        void IncludeGroup(
            _In_ winrt::Windows::Devices::Midi2::MidiGroup const& value) noexcept { m_includedGroup = value; }

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiChannel> IncludeChannels();


        void Initialize();
        void OnEndpointConnectionOpened();
        void Cleanup();

        void ProcessIncomingMessage(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args, 
            _Out_ bool& skipFurtherListeners, 
            _Out_ bool& skipMainMessageReceivedEvent);


    private:
        hstring m_id{};
        hstring m_name{};
        bool m_enabled{ false };
        IInspectable m_tag{ nullptr };
        IMidiInputConnection m_inputConnection;

        winrt::Windows::Devices::Midi2::MidiGroup m_includedGroup{ nullptr };

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiChannelEndpointListener : MidiChannelEndpointListenerT<MidiChannelEndpointListener, implementation::MidiChannelEndpointListener>
    {
    };
}
