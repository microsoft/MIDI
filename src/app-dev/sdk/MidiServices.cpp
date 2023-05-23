// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services Client SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiServices.h"
#include "MidiServices.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    bool MidiServices::CheckForWindowsMidiServices(winrt::Microsoft::Devices::Midi2::WindowsMidiServicesCheckError& errorResult)
    {
        throw hresult_not_implemented();
    }
    hstring MidiServices::GetInstalledWindowsMidiServicesVersion()
    {
        throw hresult_not_implemented();
    }
    hstring MidiServices::SdkVersion()
    {
        throw hresult_not_implemented();
    }
    hstring MidiServices::MinimumCompatibleMidiServicesVersion()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Uri MidiServices::LatestMidiServicesInstallUri()
    {
        throw hresult_not_implemented();
    }

}
