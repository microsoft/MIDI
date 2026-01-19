// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBayRouteUpdateConfig.h"
#include "VirtualPatchBay.MidiVirtualPatchBayRouteUpdateConfig.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    _Use_decl_annotations_
    MidiVirtualPatchBayRouteUpdateConfig::MidiVirtualPatchBayRouteUpdateConfig(
        winrt::guid const& routeId)
    {
        m_routeId = routeId;

    }






    json::JsonObject MidiVirtualPatchBayRouteUpdateConfig::GetConfigJson()
    {
        throw hresult_not_implemented();
    }
}
