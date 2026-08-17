// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Transports.Virtual.MidiVirtualDeviceClientEndpointInUseChangedEventArgs.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::Virtual::implementation
{
    struct MidiVirtualDeviceClientEndpointInUseChangedEventArgs : MidiVirtualDeviceClientEndpointInUseChangedEventArgsT<MidiVirtualDeviceClientEndpointInUseChangedEventArgs>
    {
        MidiVirtualDeviceClientEndpointInUseChangedEventArgs() = default;

        bool IsClientEndpointInUse() const noexcept { return m_isClientEndpointInUse; }

        void InternalInitialize(_In_ bool const isClientEndpointInUse) noexcept
        {
            m_isClientEndpointInUse = isClientEndpointInUse;
        }

    private:
        bool m_isClientEndpointInUse{ false };
    };
}
