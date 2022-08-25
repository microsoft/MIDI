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

        const bool skipEnumeration = false;

        MidiEnumeratorCreateResult enumResult = MidiEnumerator::Create(skipEnumeration);

        if (enumResult.Success)
        {
            std::unique_ptr<MidiEnumerator> enumerator = std::make_unique<MidiEnumerator>(enumResult.Enumerator);

            std::wstring deviceName = L"Foo Synth 2000";

            // find the device we're interested in. Can list all, or find by
            // either the device-supplied original name or the user-updatable name. 
            // Can also directly access a single device by its ID. Typically
            // code would present the user with a list of all devices and their 
            // streams.
            // It's possible to have multiple devices with the same name, so the
            // non-ID accessors return a collection of results.

            //auto devices = enumerator->GetStaticDeviceList();
            auto deviceList = enumerator->GetStaticDeviceListByDeviceSuppliedName(deviceName.c_str());

            if (deviceList.Count() > 0)
            {

                // Open a device
                auto deviceOpenResult = session->OpenDevice(deviceList[0].DeviceId);



                // Open a stream on that device, and listen for incoming messages


                // Send a MIDI message to the device




            }

        }


        // when the app or other scope closes, it's good practice to close the session.
        // This releases service-side resources
        session->Close();
    }
}
