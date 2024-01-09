// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiInPort.g.h"


namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiInPort : MidiInPortT<MidiInPort>
    {
        MidiInPort() = default;

        hstring DeviceId();
        winrt::event_token MessageReceived(winrt::Windows::Foundation::TypedEventHandler<winrt::MIDI_ROOT_NAMESPACE_CPP::MidiInPort, winrt::MIDI_ROOT_NAMESPACE_CPP::MidiMessageReceivedEventArgs> const& handler);
        void MessageReceived(winrt::event_token const& token) noexcept;
        void Close();
    };
}
