// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiServices.h"
#include "MidiServices.g.cpp"

#include "midi_app_sdk_version.h"

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
        return MIDI_APP_SDK_VERSION_STRING;
    }
    hstring MidiServices::MinimumCompatibleMidiServicesVersion()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Uri MidiServices::LatestMidiServicesInstallUri()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiTransportInformation> MidiServices::GetInstalledTransports()
    {
        throw hresult_not_implemented();
    }
}
