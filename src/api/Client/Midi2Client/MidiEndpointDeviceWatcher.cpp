// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointDeviceWatcher.h"
#include "MidiEndpointDeviceWatcher.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{


    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::Start()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    void MidiEndpointDeviceWatcher::Stop()
    {
        throw hresult_not_implemented();
    }

    _Use_decl_annotations_
    collections::IMap<winrt::hstring, midi2::MidiEndpointDeviceInformation> MidiEndpointDeviceWatcher::EndpointDevices()
    {
        throw hresult_not_implemented();
    }



}
