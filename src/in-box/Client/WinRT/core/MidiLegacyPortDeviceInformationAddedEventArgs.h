// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.Legacy.MidiLegacyPortDeviceInformationAddedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationAddedEventArgs : MidiLegacyPortDeviceInformationAddedEventArgsT<MidiLegacyPortDeviceInformationAddedEventArgs>
    {
        MidiLegacyPortDeviceInformationAddedEventArgs() = default;

        legacy::MidiLegacyPortDeviceInformation AddedDevice() const noexcept { return m_device; }


        void InternalInitialize(_In_ legacy::MidiLegacyPortDeviceInformation const& addedDevice) noexcept
        {
            m_device = addedDevice;
        }

    private:
        legacy::MidiLegacyPortDeviceInformation m_device{ nullptr };

    };
}
