// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceInformation.h"
#include "MidiEndpointDeviceInformation.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiEndpointDeviceInformation MidiEndpointDeviceInformation::CreateFromId(winrt::hstring const& /*id*/) noexcept
    {
        // TODO
        return nullptr;
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdateFromDeviceInformation(Windows::Devices::Enumeration::DeviceInformation const& /*info*/) noexcept
    {

    }

    _Use_decl_annotations_
    void MidiEndpointDeviceInformation::InternalUpdate(winrt::hstring const& /*deviceId*/) noexcept
    {

    }


}
