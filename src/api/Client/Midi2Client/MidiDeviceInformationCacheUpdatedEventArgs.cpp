// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiDeviceInformationCacheUpdatedEventArgs.h"
#include "MidiDeviceInformationCacheUpdatedEventArgs.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{
    winrt::Windows::Devices::Midi2::MidiCacheUpdateType MidiDeviceInformationCacheUpdatedEventArgs::UpdateType()
    {
        throw hresult_not_implemented();
    }
    hstring MidiDeviceInformationCacheUpdatedEventArgs::DeviceId()
    {
        throw hresult_not_implemented();
    }
    hstring MidiDeviceInformationCacheUpdatedEventArgs::Key()
    {
        throw hresult_not_implemented();
    }
}
