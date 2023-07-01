// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiServices.g.h"

#include "MidiTransportInformation.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiServices : MidiServicesT<MidiServices>
    {
        MidiServices() = default;

        static winrt::Microsoft::Devices::Midi2::WindowsMidiServicesCheckResult CheckForWindowsMidiServices();
        static hstring GetInstalledWindowsMidiServicesVersion();
        static hstring SdkVersion();
        static hstring MinimumCompatibleMidiServicesVersion();
        static winrt::Windows::Foundation::Uri LatestMidiServicesInstallUri();
        static winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiTransportInformation> GetInstalledTransports();

    private:

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiServices : MidiServicesT<MidiServices, implementation::MidiServices>
    {
    };
}
