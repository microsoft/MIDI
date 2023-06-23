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

    // check for presence of compatible Windows MIDI Services

    WindowsMidiServicesCheckError err;
    bool servicesOk = MidiServices::CheckForWindowsMidiServices(err);

    // proceed only if MIDI services is present and compatible. Your own application may decide
    // to fall back to WinRT/WinMM MIDI 1.0 APIs, or to prompt the user to install the latest 
    // version of Windows MIDI Services

    if (servicesOk)
    {
        // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
        // more than one session. If so, the session name should be meaningful to the user, like
        // the name of a browser tab, or a project.

        MidiSessionSettings sessionSettings = MidiSessionSettings::Default();

        auto session = MidiSession::CreateNewSession(L"Sample Session", sessionSettings);

        // you can ask for MIDI 1.0 Byte Stream devices, MIDI 2.0 UMP devices, or both. Note that Some MIDI 2.0
        // endpoints may have MIDI 1.0 function blocks in them, so this is endpoint/device-level only.
        // Note that every device uses UMP through this API, but it can be helpful to know when a device is
        // a MIDI 1.0 device at the main interface level.

        hstring deviceSelector = MidiEndpointConnection::GetDeviceSelector();

        // Enumerate UMP endpoints. Note that per C++, main cannot be a co-routine,
        // so we can't just co_await this async call, but instead use the C++/WinRT Extension "get()". 
        // We may end up wrapping this enumeration code into another class to instantly transform to 
        // MidiDeviceInformation instances, and to simplify calling code (reducing need for apps to
        // incorporate async handling).

        Windows::Foundation::IAsyncOperation<DeviceInformationCollection> op = DeviceInformation::FindAllAsync(deviceSelector);
        DeviceInformationCollection endpointDevices = op.get();

        if (endpointDevices.Size() > 0)
        {
            // We're going to just pick the first one we find. Normally, you'd have the user pick from a list
            // or you'd otherwise have an Id at hand.
            DeviceInformation selectedEndpointInformation = endpointDevices.GetAt(0);

            // if we want the additional properties that are available to us, we can wrap the
            // DeviceInformation object with a MIDI-specific one. You can also skip this and call the CreatFromId 
            // method directly on MidiDeviceInformation if you have an Id handy.
            MidiDeviceInformation selectedMidiEndpointInformation = MidiDeviceInformation::FromDeviceInformation(selectedEndpointInformation);

            //selectedMidiEndpointInformation.DeviceThumbnail();
            //selectedMidiEndpointInformation.EndpointDataFormat();
            // ...

            // then you connect to the UMP endpoint
            auto endpoint = session.ConnectToEndpoint(selectedMidiEndpointInformation.Id(), true, MidiEndpointConnectOptions::Default());

            // after connecting, you can send and receive messages and more

            // ...


        }
        else
        {
            // no MIDI endpoints found. We'll just bail here
        }

        // close the session, detaching all Windows MIDI Services resources
        session.Close();
    }
    else
    {
        if (err == WindowsMidiServicesCheckError::NotPresent)
        {
            // allow the user to install the minimum required version
        }
        else if (err == WindowsMidiServicesCheckError::IncompatibleVersion)
        {
            // allow the user to install the minimum required version
        }
    }

}
