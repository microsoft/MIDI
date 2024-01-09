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
        winrt::event_token MessageReceived(_In_ foundation::TypedEventHandler<midi1::MidiInPort, midi1::MidiMessageReceivedEventArgs> const& handler);
        void MessageReceived(_In_ winrt::event_token const& token) noexcept;
        void Close();
    };
}
