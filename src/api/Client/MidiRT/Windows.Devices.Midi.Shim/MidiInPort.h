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
    struct MidiInPort : MidiInPortT<MidiInPort, IMidiCallback>
    {
    public:
        MidiInPort() = default;

        static foundation::IAsyncOperation<midi1::MidiInPort> FromIdAsync(_In_ winrt::hstring deviceId);
        static winrt::hstring GetDeviceSelector();

        winrt::hstring DeviceId();

        winrt::event_token MessageReceived(_In_ foundation::TypedEventHandler<midi1::MidiInPort, midi1::MidiMessageReceivedEventArgs> const& handler);
        void MessageReceived(_In_ winrt::event_token const& token) noexcept;

        void Close();


        STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG timestamp, _In_ LONGLONG context) override;

    private:


    };
}

namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiInPort : MidiInPortT<MidiInPort, implementation::MidiInPort>
    {
    };
}
