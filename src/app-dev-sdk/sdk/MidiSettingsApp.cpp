// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSettingsApp.h"
#include "MidiSettingsApp.g.cpp"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    bool MidiSettingsApp::IsSettingsAppInstalled()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Foundation::Uri MidiSettingsApp::SettingsAppInstallUri()
    {
        throw hresult_not_implemented();
    }
    void MidiSettingsApp::ShowEndpointPropertiesWindow(hstring const& endpointId, winrt::Microsoft::Devices::Midi2::MidiSettingsAppDisplayMode const& displayMode)
    {
        throw hresult_not_implemented();
    }
    hstring MidiSettingsApp::ShowEndpointCreationWindow(hstring const& transportId, winrt::Microsoft::Devices::Midi2::MidiSettingsAppDisplayMode const& displayMode)
    {
        throw hresult_not_implemented();
    }
}
