// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once
#include "MidiEndpointDeviceInformationAddedEventArgs.g.h"


namespace winrt::Windows::Devices::Midi2::Enumeration::implementation
{
    struct MidiEndpointDeviceInformationAddedEventArgs : MidiEndpointDeviceInformationAddedEventArgsT<MidiEndpointDeviceInformationAddedEventArgs>
    {
        MidiEndpointDeviceInformationAddedEventArgs() = default;

        midi2enum::MidiEndpointDeviceInformation AddedDevice() const noexcept { return m_addedDevice; }


        void InternalInitialize(
            _In_ midi2enum::MidiEndpointDeviceInformation const& addedDevice
        ) noexcept;


    private:
        midi2enum::MidiEndpointDeviceInformation m_addedDevice{ nullptr };

    };
}
