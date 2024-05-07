// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiEndpointDeviceInformationAddedEventArgs.g.h"


namespace MIDI_CPP_NAMESPACE::implementation
{
    struct MidiEndpointDeviceInformationAddedEventArgs : MidiEndpointDeviceInformationAddedEventArgsT<MidiEndpointDeviceInformationAddedEventArgs>
    {
        MidiEndpointDeviceInformationAddedEventArgs() = default;

        midi2::MidiEndpointDeviceInformation AddedDevice() const noexcept { return m_addedDevice; }


        void InternalInitialize(
            _In_ midi2::MidiEndpointDeviceInformation const& addedDevice
        ) noexcept;


    private:
        midi2::MidiEndpointDeviceInformation m_addedDevice{ nullptr };

    };
}
