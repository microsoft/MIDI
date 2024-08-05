// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBayManager.h"
#include "MidiVirtualPatchBayManager.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    bool MidiVirtualPatchBayManager::IsTransportAvailable()
    {
        throw hresult_not_implemented();
    }
    winrt::guid MidiVirtualPatchBayManager::AbstractionId()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    local::MidiVirtualPatchBayRouteCreationResult MidiVirtualPatchBayManager::CreateRoute(
        local::MidiVirtualPatchBayRouteCreationConfig const& creationConfig)
    {
        UNREFERENCED_PARAMETER(creationConfig);

        return {};
    }

    _Use_decl_annotations_
    bool MidiVirtualPatchBayManager::RemoveRoute(
        local::MidiVirtualPatchBayRouteRemovalConfig const& removalConfig)
    {
        UNREFERENCED_PARAMETER(removalConfig);

        return false;
    }

    _Use_decl_annotations_
    local::MidiVirtualPatchBayRouteUpdateResult MidiVirtualPatchBayManager::UpdateRoute(
        local::MidiVirtualPatchBayRouteUpdateConfig const& updateConfig)
    {
        UNREFERENCED_PARAMETER(updateConfig);

        return {};
    }

    collections::IVector<local::MidiVirtualPatchBayRouteDefinition> MidiVirtualPatchBayManager::GetRoutes()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    local::MidiVirtualPatchBayRouteDefinition MidiVirtualPatchBayManager::GetRoute(
        winrt::guid routeId)
    {
        UNREFERENCED_PARAMETER(routeId);

        throw hresult_not_implemented();
    }


}
