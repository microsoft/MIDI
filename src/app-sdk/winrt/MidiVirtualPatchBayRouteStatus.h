// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "VirtualPatchBay.MidiVirtualPatchBayRouteStatus.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    struct MidiVirtualPatchBayRouteStatus : MidiVirtualPatchBayRouteStatusT<MidiVirtualPatchBayRouteStatus>
    {
        MidiVirtualPatchBayRouteStatus() = default;

        winrt::guid RouteId() { return m_routeId; }
        bool IsEnabled() { return m_isEnabled; }


    private:
        winrt::guid m_routeId;
        bool m_isEnabled;



    };
}
