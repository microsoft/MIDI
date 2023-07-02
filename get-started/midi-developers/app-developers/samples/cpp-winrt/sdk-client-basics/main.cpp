// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include "pch.h"
#include <iostream>

using namespace winrt;
using namespace Microsoft::Devices::Midi2;
using namespace Windows::Devices::Enumeration;

int main()
{
    init_apartment();

    std::cout << "Checking for MIDI Services" << std::endl;

    // check for presence of compatible Windows MIDI Services
    auto checkResult = MidiServices::CheckForWindowsMidiServices();

    // proceed only if MIDI services is present and compatible. Your own application may decide
    // to fall back to WinRT/WinMM MIDI 1.0 APIs, or to prompt the user to install the latest 
    // version of Windows MIDI Services

    if (checkResult == WindowsMidiServicesCheckResult::PresentAndUsable)
    {
        std::cout << "MIDI Services Present and usable" << std::endl;

        // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
        // more than one session. If so, the session name should be meaningful to the user, like
        // the name of a browser tab, or a project.

        std::cout << "Creating session settings." << std::endl;

        MidiSessionSettings sessionSettings = MidiSessionSettings::Default();

        std::cout << "Creating session." << std::endl;

        auto session = MidiSession::CreateNewSession(L"Sample Session", sessionSettings);

        // you can ask for MIDI 1.0 Byte Stream devices, MIDI 2.0 UMP devices, or both. Note that Some MIDI 2.0
        // endpoints may have MIDI 1.0 function blocks in them, so this is endpoint/device-level only.
        // Note that every device uses UMP through this API, but it can be helpful to know when a device is
        // a MIDI 1.0 device at the main interface level.

        std::cout << "Creating Device Selector." << std::endl;

        hstring deviceSelector = MidiEndpointConnection::GetDeviceSelector();

        // Enumerate UMP endpoints. Note that per C++, main cannot be a co-routine,
        // so we can't just co_await this async call, but instead use the C++/WinRT Extension "get()". 
        // We may end up wrapping this enumeration code into another class to instantly transform to 
        // MidiDeviceInformation instances, and to simplify calling code (reducing need for apps to
        // incorporate async handling).

        std::cout << "Enumerating through Windows::Devices::Enumeration." << std::endl;

        Windows::Foundation::IAsyncOperation<DeviceInformationCollection> op = DeviceInformation::FindAllAsync(deviceSelector);
        DeviceInformationCollection endpointDevices = op.get();

        if (endpointDevices.Size() > 0)
        {
            std::cout << "MIDI Endpoints were found (not really, but pretending they are for now)." << std::endl;

            // We're going to just pick the first one we find. Normally, you'd have the user pick from a list
            // or you'd otherwise have an Id at hand.
            DeviceInformation selectedEndpointInformation = endpointDevices.GetAt(0);

            // if we want the additional properties that are available to us, we can wrap the
            // DeviceInformation object with a MIDI-specific one. You can also skip this and call the CreatFromId 
            // method directly on MidiDeviceInformation if you have an Id handy.
//            MidiDeviceInformation selectedMidiEndpointInformation = MidiDeviceInformation::FromDeviceInformation(selectedEndpointInformation);

            //selectedMidiEndpointInformation.DeviceThumbnail();
            //selectedMidiEndpointInformation.EndpointDataFormat();
            // ...

            // then you connect to the UMP endpoint
            std::cout << "Connecting to UMP Endpoint." << std::endl;

            //auto endpoint = session.ConnectToEndpoint(selectedMidiEndpointInformation.Id(), MidiEndpointConnectOptions::Default());
            auto endpoint = session.ConnectToEndpoint(L"foobarbaz", MidiEndpointConnectOptions::Default());

            // after connecting, you can send and receive messages

//            auto writer = endpoint.GetMessageWriter();

            // writer.WriteUmpWithTimestamp(...);


        }
        else
        {
            // no MIDI endpoints found. We'll just bail here
            std::cout << "No MIDI Endpoints were found." << std::endl;
        }

        // close the session, detaching all Windows MIDI Services resources and closing all connections
        // You can also disconnect individual Endpoint Connections when you are done with them
        session.Close();
    }
    else
    {
        if (checkResult == WindowsMidiServicesCheckResult::NotPresent)
        {
            std::cout << "MIDI Services Not Present" << std::endl;

            // allow the user to install the minimum required version
        }
        else if (checkResult == WindowsMidiServicesCheckResult::IncompatibleVersion)
        {
            std::cout << "MIDI Present, but is not a compatible version." << std::endl;
            std::cout << "Here you would prompt the user to install the latest version from " << winrt::to_string(MidiServices::LatestMidiServicesInstallUri().ToString()) << std::endl;

            // allow the user to install the minimum required version
        }
    }

}
