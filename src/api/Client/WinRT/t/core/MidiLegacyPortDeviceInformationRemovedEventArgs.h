// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Enumeration.Legacy.MidiLegacyPortDeviceInformationRemovedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    struct MidiLegacyPortDeviceInformationRemovedEventArgs : MidiLegacyPortDeviceInformationRemovedEventArgsT<MidiLegacyPortDeviceInformationRemovedEventArgs>
    {
        MidiLegacyPortDeviceInformationRemovedEventArgs() = default;

        legacy::MidiLegacyPortDeviceInformation RemovedDevice() const noexcept { return m_device; }

        void InternalInitialize(_In_ legacy::MidiLegacyPortDeviceInformation const& device) noexcept
        {
            m_device = device;
        }

    private:
        legacy::MidiLegacyPortDeviceInformation m_device{ nullptr };

    };
}
