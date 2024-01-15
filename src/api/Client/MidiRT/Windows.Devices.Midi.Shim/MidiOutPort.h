// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiOutPort.g.h"

namespace winrt::MIDI_ROOT_NAMESPACE_CPP::implementation
{
    struct MidiOutPort : MidiOutPortT<MidiOutPort>
    {
        MidiOutPort();

        static foundation::IAsyncOperation<midi1::IMidiOutPort> FromIdAsync(_In_ winrt::hstring deviceId);
        static winrt::hstring GetDeviceSelector();

        winrt::hstring DeviceId() { return m_deviceId; }
        void SendMessage(_In_ midi1::IMidiMessage const& midiMessage);
        void SendBuffer(_In_ streams::IBuffer const& midiData);

        void Close();

        void InternalInitialize(_In_ winrt::hstring deviceId, _In_ winrt::com_ptr<IMidiOut> endpoint)
        {
            m_endpoint = endpoint;
            m_deviceId = deviceId;
        }

    private:
        winrt::hstring m_deviceId{};
        winrt::com_ptr<IMidiOut> m_endpoint{};
    };
}

namespace winrt::MIDI_ROOT_NAMESPACE_CPP::factory_implementation
{
    struct MidiOutPort : MidiOutPortT<MidiOutPort, implementation::MidiOutPort>
    {
    };
}