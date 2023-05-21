// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
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
    bool MidiServices::IsOpen()
    {
        throw hresult_not_implemented();
    }
}
