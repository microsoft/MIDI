// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------


#include <iostream>
#include <memory>

// Remember to add the include file path to your include locations in the project
// You'll also want to add the redist folder for your architecture in the lib folders
// Typically, these files follow the Windows SDK pattern, and are under 
// Program Files (x86)\Microsoft SDKs\Midi Services\(version)\cpp\include
// Finally, please remember that the API was built using C++ 20.

#include "WindowsMidiServicesApi.h"

using namespace Microsoft::Windows::Midi;
using namespace Microsoft::Windows::Midi::Enumeration;

int main()
{
    // you may want to keep this in a MIDI handling class, a global, a document object, or similar
    // scope is up to you, as is cleanup of the pointer.
    std::unique_ptr<MidiSession> session;   

    std::cout << "MIDI Basic use sample" << std::endl;

    MidiSessionCreateResult result = MidiSession::Create(L"My Project", L"Basic Use Sample");

    if (result.Success)
    {
        session = std::make_unique<MidiSession>(result.Session);

        // if the name changes (like when the user renames the file they are working on)
        // you can change the name of the session by calling UpdateName. That will make
        // a service call to update the session registration, viewable from the 
        // end-user settings tools
        session->UpdateName(L"New Session Name");

        // Next, enumerate devices. This doesn't cause a re-enumeration if the devices
        // have already been enumerated by another application, or earlier in this
        // application.



        // Open a device

        // Open a stream on that device, and listen for incoming messages


        // Send a MIDI message to the device



        // when the app or other scope closes, it's good practice to close the session.
        // This releases service-side resources
        session->Close();
    }
}
