// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiVirtualPatchBayManager.h"
#include "VirtualPatchBay.MidiVirtualPatchBayManager.g.cpp"

namespace winrt::Microsoft::Windows::Devices::Midi2::VirtualPatchBay::implementation
{
    bool MidiVirtualPatchBayManager::IsTransportAvailable()
    {
        throw hresult_not_implemented();
    }
    winrt::guid MidiVirtualPatchBayManager::TransportId()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    vpb::MidiVirtualPatchBayRouteCreationResult MidiVirtualPatchBayManager::CreateRoute(
        vpb::MidiVirtualPatchBayRouteCreationConfig const& creationConfig)
    {
        UNREFERENCED_PARAMETER(creationConfig);

        return {};
    }

    _Use_decl_annotations_
    bool MidiVirtualPatchBayManager::RemoveRoute(
        vpb::MidiVirtualPatchBayRouteRemovalConfig const& removalConfig)
    {
        UNREFERENCED_PARAMETER(removalConfig);

        return false;
    }

    _Use_decl_annotations_
    vpb::MidiVirtualPatchBayRouteUpdateResult MidiVirtualPatchBayManager::UpdateRoute(
            vpb::MidiVirtualPatchBayRouteUpdateConfig const& updateConfig)
    {
        UNREFERENCED_PARAMETER(updateConfig);

        return {};
    }

    collections::IVectorView<vpb::MidiVirtualPatchBayRouteDefinition> MidiVirtualPatchBayManager::GetRoutes()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    vpb::MidiVirtualPatchBayRouteDefinition MidiVirtualPatchBayManager::GetRoute(
        winrt::guid const& routeId)
    {
        UNREFERENCED_PARAMETER(routeId);

        throw hresult_not_implemented();
    }


    _Use_decl_annotations_
    vpb::MidiVirtualPatchBayRouteStatus MidiVirtualPatchBayManager::GetRouteStatus(
        winrt::guid const& routeId)
    {
        UNREFERENCED_PARAMETER(routeId);

        throw hresult_not_implemented();
    }


}
