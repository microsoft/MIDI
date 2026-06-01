// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiLegacyPortDeviceInformationUpdatedEventArgs.h"
#include "Legacy.MidiLegacyPortDeviceInformationUpdatedEventArgs.g.cpp"

namespace winrt::Windows::Devices::Midi2::Enumeration::Legacy::implementation
{
    hstring MidiLegacyPortDeviceInformationUpdatedEventArgs::PortDeviceId()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Enumeration::DeviceInformationUpdate MidiLegacyPortDeviceInformationUpdatedEventArgs::DeviceInformationUpdate()
    {
        throw hresult_not_implemented();
    }
    bool MidiLegacyPortDeviceInformationUpdatedEventArgs::IsNameUpdated()
    {
        throw hresult_not_implemented();
    }
    bool MidiLegacyPortDeviceInformationUpdatedEventArgs::IsPortNumberUpdated()
    {
        throw hresult_not_implemented();
    }
}
