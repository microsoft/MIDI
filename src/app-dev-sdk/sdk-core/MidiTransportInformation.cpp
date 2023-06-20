// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiTransportInformation.h"
#include "MidiTransportInformation.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    hstring MidiTransportInformation::Id()
    {
        throw hresult_not_implemented();
    }
    hstring MidiTransportInformation::Name()
    {
        throw hresult_not_implemented();
    }
    hstring MidiTransportInformation::ShortName()
    {
        throw hresult_not_implemented();
    }
    hstring MidiTransportInformation::IconPath()
    {
        throw hresult_not_implemented();
    }
    hstring MidiTransportInformation::Author()
    {
        throw hresult_not_implemented();
    }
    hstring MidiTransportInformation::ServicePluginFileName()
    {
        throw hresult_not_implemented();
    }
    bool MidiTransportInformation::IsRuntimeCreatable()
    {
        throw hresult_not_implemented();
    }
}
