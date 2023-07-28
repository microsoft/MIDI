// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiStreamMessageEndpointListener.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiStreamMessageEndpointListener : MidiStreamMessageEndpointListenerT<MidiStreamMessageEndpointListener>
    {
        MidiStreamMessageEndpointListener() = default;

        hstring Id() { return m_id; }
        void Id(hstring const& value) { m_id = value; }
        hstring Name() { return m_name; }
        void Name(hstring const& value) { m_name = value; }
        winrt::Windows::Foundation::IInspectable Tag() { return m_tag; }
        void Tag(winrt::Windows::Foundation::IInspectable const& value) { m_tag = value; }

        void ProcessIncomingMessage(winrt::Windows::Devices::Midi2::IMidiInputConnection const& sourceConnection, winrt::Windows::Devices::Midi2::MidiMessageReceivedEventArgs const& args, bool& stopProcessing, bool& removeMessageFromQueue);

    private:
        hstring m_id{};
        hstring m_name{};
        IInspectable m_tag{ nullptr };
    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiStreamMessageEndpointListener : MidiStreamMessageEndpointListenerT<MidiStreamMessageEndpointListener, implementation::MidiStreamMessageEndpointListener>
    {
    };
}
