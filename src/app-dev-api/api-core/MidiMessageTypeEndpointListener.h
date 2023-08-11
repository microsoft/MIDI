// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiMessageTypeEndpointListener.g.h"

#include "string_util.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiMessageTypeEndpointListener : MidiMessageTypeEndpointListenerT<MidiMessageTypeEndpointListener>
    {
        MidiMessageTypeEndpointListener() = default;

        hstring Id() const { return m_id; }
        void Id(hstring const& value) { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        hstring Name() const { return m_name; }
        void Name(_In_ hstring const& value) { m_name = internal::TrimmedHStringCopy(value); }

        winrt::Windows::Foundation::IInspectable Tag() const { return m_tag; }
        void Tag(_In_ winrt::Windows::Foundation::IInspectable const& value) { m_tag = value; }

        winrt::Windows::Devices::Midi2::IMidiInputConnection InputConnection() const { return m_inputConnection; }
        void InputConnection(_In_ winrt::Windows::Devices::Midi2::IMidiInputConnection const& value) { m_inputConnection = value; }

        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiUmpMessageType> IncludeMessageTypes();

        void Initialize();
        void Cleanup();

        void ProcessIncomingMessage(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args,
            _Out_ bool& skipFurtherListeners,
            _Out_ bool& skipMainMessageReceivedEvent);

        winrt::Windows::Foundation::IAsyncAction ProcessIncomingMessageAsync(
            _In_ winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs args);


    private:
        hstring m_id{};
        hstring m_name{};
        IInspectable m_tag{ nullptr };
        IMidiInputConnection m_inputConnection;

    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiMessageTypeEndpointListener : MidiMessageTypeEndpointListenerT<MidiMessageTypeEndpointListener, implementation::MidiMessageTypeEndpointListener>
    {
    };
}
