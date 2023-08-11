// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiGroupEndpointListener.g.h"

#include "string_util.h"

namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener>
    {
        MidiGroupEndpointListener() = default;

        hstring Id() { return m_id; }
        void Id(hstring const& value) { m_id = internal::ToUpperTrimmedHStringCopy(value); }

        hstring Name() { return m_name; }
        void Name(_In_ hstring const& value) { m_name = internal::TrimmedHStringCopy(value); }

        winrt::Windows::Foundation::IInspectable Tag() { return m_tag; }
        void Tag(_In_ winrt::Windows::Foundation::IInspectable const& value) { m_tag = value; }


        winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::MidiGroup> IncludeGroups();

        void Initialize(
            _In_ winrt::Windows::Devices::Midi2::IMidiInputConnection const& inputConnection);
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


    };
}
namespace winrt::Windows::Devices::Midi2::factory_implementation
{
    struct MidiGroupEndpointListener : MidiGroupEndpointListenerT<MidiGroupEndpointListener, implementation::MidiGroupEndpointListener>
    {
    };
}
