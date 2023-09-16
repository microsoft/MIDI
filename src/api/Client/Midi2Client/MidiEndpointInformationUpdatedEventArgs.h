// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointInformationUpdatedEventArgs.g.h"


namespace winrt::Windows::Devices::Midi2::implementation
{
    struct MidiEndpointInformationUpdatedEventArgs : MidiEndpointInformationUpdatedEventArgsT<MidiEndpointInformationUpdatedEventArgs>
    {
        MidiEndpointInformationUpdatedEventArgs() = default;

        winrt::hstring DeviceId() { return m_deviceId; }
    private:
        winrt::hstring m_deviceId{};
    };
}
