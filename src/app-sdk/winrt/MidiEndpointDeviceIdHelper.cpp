// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceIdHelper.h"
#include "MidiEndpointDeviceIdHelper.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::GetShortIdFromFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        
        // TEMP
        return fullEndpointDeviceId;
    }

    _Use_decl_annotations_
    winrt::hstring MidiEndpointDeviceIdHelper::GetFullIdFromShortId(winrt::hstring const& shortEndpointDeviceId) noexcept
    {
        // TEMP
        return shortEndpointDeviceId;
    }

    _Use_decl_annotations_
    winrt::guid MidiEndpointDeviceIdHelper::GetInterfaceIdFromFullId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        // TEMP
        UNREFERENCED_PARAMETER(fullEndpointDeviceId);
        return foundation::GuidHelper::Empty();
    }

    _Use_decl_annotations_
    bool MidiEndpointDeviceIdHelper::IsWindowsMidiServicesDeviceId(winrt::hstring const& fullEndpointDeviceId) noexcept
    {
        // TEMP
        UNREFERENCED_PARAMETER(fullEndpointDeviceId);
        return false;
    }

}
