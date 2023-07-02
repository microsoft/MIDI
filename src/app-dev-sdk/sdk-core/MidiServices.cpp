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

using namespace Microsoft::Devices::Midi2;

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    winrt::Microsoft::Devices::Midi2::WindowsMidiServicesCheckResult MidiServices::CheckForWindowsMidiServices()
    {
        // TODO: This will be a call to check the actual service

        return WindowsMidiServicesCheckResult::PresentAndUsable;
    }
    hstring MidiServices::GetInstalledWindowsMidiServicesVersion()
    {
        // TODO: Temp

        return L"0.0.0-not-present";
    }
    hstring MidiServices::SdkVersion()
    {
        return MIDI_APP_SDK_VERSION_STRING;
    }
    hstring MidiServices::MinimumCompatibleMidiServicesVersion()
    {
        return MIDI_MINIMUM_REQUIRED_SERVICES_VERSION_STRING;
    }
    winrt::Windows::Foundation::Uri MidiServices::LatestMidiServicesInstallUri()
    {
        return Windows::Foundation::Uri(MIDI_SERVICES_INSTALL_URI_STRING);
    }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Devices::Midi2::MidiTransportInformation> MidiServices::GetInstalledTransports()
    {
        // TODO: need to get an actual list of installed transports

        auto transports = winrt::single_threaded_vector<winrt::Microsoft::Devices::Midi2::MidiTransportInformation>();

        // we need the impl type to call this constructor
        com_ptr<implementation::MidiTransportInformation> transport1 = 
            winrt::make_self<implementation::MidiTransportInformation>(
            L"SWD//USB/DUMMY-TRANSPORT",    // Id
            L"Dummy USB Transport",         // Name
            L"USB",                         // short name
            L"",                            // icon path
            L"Microsoft",                   // author
            L"foo_usbmidi2.dll",            // service plugin file name
            false                           // is runtime creatable
        );
            
        // append projected instance
        transports.Append(*transport1);


        auto transport2 = winrt::make_self<MidiTransportInformation>(
            L"SWD//VIRT/DUMMY-TRANSPORT",   // Id
            L"Dummy Virtual Transport",     // Name
            L"VIRT",                        // short name
            L"",                            // icon path
            L"Microsoft",                   // author
            L"foo_virt.dll",                // service plugin file name
            true                            // is runtime creatable
        );

        transports.Append(*transport2);



        return transports;
    }
}
