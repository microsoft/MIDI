// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


bool MidiLegacyPortDeviceWatcherTests::ClassSetup()
{
    StartWinRTMTA();
    return true;
}

bool MidiLegacyPortDeviceWatcherTests::ClassCleanup()
{
    ShutdownWinRT();
    return true;
}


void MidiLegacyPortDeviceWatcherTests::TestCreateAndEnumerateForAllFlows()
{
    std::string flowName;

    for (auto const& flow : { Midi1PortFlow::MidiMessageSource, Midi1PortFlow::MidiMessageDestination })
    {
        if (flow == Midi1PortFlow::MidiMessageSource)
        {
            flowName = "MidiMessageSource";
        }
        else
        {
            flowName = "MidiMessageDestination";
        }

        std::cout << "Testing flow: " << flowName << std::endl;

        wil::unique_event_nothrow allEnumerationCompleted;
        allEnumerationCompleted.create();

        auto watcher = MidiLegacyPortDeviceWatcher::CreateForFlow(flow);
        VERIFY_IS_NOT_NULL(watcher);

        uint32_t countAdded{ 0 };
        uint32_t countRemoved{ 0 };

        auto addedToken = watcher.Added([&](auto const& source, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
            {
                countAdded++;

                VERIFY_IS_NOT_NULL(source);
                VERIFY_IS_NOT_NULL(args);

                //std::cout << "Watcher Added event. Flow: " << flowName << ", Port ID: " << winrt::to_string(args.AddedDevice().PortDeviceId()) << std::endl;
                //std::cout << " -- Name:" << winrt::to_string(args.AddedDevice().Name()) << std::endl;
                //std::cout << std::endl;
            });

        auto removedToken = watcher.Removed([&](auto const& source, MidiLegacyPortDeviceInformationRemovedEventArgs  const& args)
            {
                countRemoved++;

                VERIFY_IS_NOT_NULL(source);
                VERIFY_IS_NOT_NULL(args);

                //std::cout << "Watcher Removed event. Flow: " << flowName << ", Port ID: " << winrt::to_string(args.RemovedDevice().PortDeviceId()) << std::endl;
                //std::cout << std::endl;
            });

        auto updatedToken = watcher.Updated([&](auto const& source, MidiLegacyPortDeviceInformationUpdatedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(source);
                VERIFY_IS_NOT_NULL(args);

                //std::cout << "Watcher Updated event. Flow: " << flowName << ", Port ID: " << winrt::to_string(args.UpdatedDevice().PortDeviceId()) << std::endl;
                //std::cout << " -- Name Updated:   " << args.IsNameUpdated() << std::endl;
                //std::cout << " -- Number Updated: " << args.IsNumberUpdated() << std::endl;
                //std::cout << std::endl;
            });

        auto enumerationCompletedToken = watcher.EnumerationCompleted([&](auto const& source, auto const& args)
            {
                // args are null as is typical for a watcher Enumeration Completed, Started, or Stopped event.
                UNREFERENCED_PARAMETER(args);
                VERIFY_IS_NOT_NULL(source);

                std::cout << "Enumeration Completed Flow: " << flowName << std::endl;

                allEnumerationCompleted.SetEvent();
            });

        auto cleanup = wil::scope_exit([&]
            {
                // cleanup
                std::cout << "Cleaning up watcher" << std::endl;

                if (watcher == nullptr) return;

                watcher.Stop();

                if (addedToken) watcher.Added(addedToken);
                if (removedToken) watcher.Removed(removedToken);
                if (updatedToken) watcher.Updated(updatedToken);
                if (enumerationCompletedToken) watcher.EnumerationCompleted(enumerationCompletedToken);
            });

        std::cout << "Starting watcher" << std::endl;

        watcher.Start();

        // Wait for incoming message
        if (!allEnumerationCompleted.wait(10000))
        {
            std::cout << "Failure waiting for enumeration complete, timed out." << std::endl;
            VERIFY_FAIL();
        }

        // make sure we collected all the ports correctly
        VERIFY_ARE_EQUAL(watcher.EnumeratedPorts().Size(), countAdded - countRemoved);

        if (flow == winrt::Windows::Devices::Midi2::Enumeration::Midi1PortFlow::MidiMessageSource)
        {
            VERIFY_IS_TRUE(watcher.CountSourcePorts() > 0);
            VERIFY_ARE_EQUAL(watcher.EnumeratedPorts().Size(), watcher.CountSourcePorts());

            // we only have a single flow, so the opposite flow should be empty
            VERIFY_IS_TRUE(watcher.CountDestinationPorts() == 0);
        }
        else
        {
            VERIFY_IS_TRUE(watcher.CountDestinationPorts() > 0);
            VERIFY_ARE_EQUAL(watcher.EnumeratedPorts().Size(), watcher.CountDestinationPorts());

            // we only have a single flow, so the opposite flow should be empty
            VERIFY_IS_TRUE(watcher.CountSourcePorts() == 0);
        }


        //allEnumerationCompleted.reset();
    }


}


void MidiLegacyPortDeviceWatcherTests::TestCreateAndEnumerate()
{
    wil::unique_event_nothrow allEnumerationCompleted;
    allEnumerationCompleted.create();

    auto watcher = MidiLegacyPortDeviceWatcher::Create();
    VERIFY_IS_NOT_NULL(watcher);

    uint32_t countAdded{ 0 };
    uint32_t countRemoved{ 0 };


    auto addedToken = watcher.Added([&](auto const& source, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            countAdded++;

            VERIFY_IS_NOT_NULL(source);
            VERIFY_IS_NOT_NULL(args);

            //std::cout << "Watcher Added event: " << winrt::to_string(args.AddedDevice().PortDeviceId()) << std::endl;
            //std::cout << " -- Name:" << winrt::to_string(args.AddedDevice().Name()) << std::endl;
            //std::cout << std::endl;
        });

    auto removedToken = watcher.Removed([&](auto const& source, MidiLegacyPortDeviceInformationRemovedEventArgs  const& args)
        {
            countRemoved++;

            VERIFY_IS_NOT_NULL(source);
            VERIFY_IS_NOT_NULL(args);

            //std::cout << "Watcher Removed event: " << winrt::to_string(args.RemovedDevice().PortDeviceId()) << std::endl;
            //std::cout << std::endl;
        });

    auto updatedToken = watcher.Updated([&](auto const& source, MidiLegacyPortDeviceInformationUpdatedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(source);
            VERIFY_IS_NOT_NULL(args);

            //std::cout << "Watcher Updated event: " << winrt::to_string(args.UpdatedDevice().PortDeviceId()) << std::endl;
            //std::cout << " -- Name Updated:   " << args.IsNameUpdated() << std::endl;
            //std::cout << " -- Number Updated: " << args.IsNumberUpdated() << std::endl;
            //std::cout << std::endl;
        });

    auto enumerationCompletedToken = watcher.EnumerationCompleted([&](auto const& source, auto const& args)
        {
            // args are null as is typical for a watcher Enumeration Completed, Started, or Stopped event.
            UNREFERENCED_PARAMETER(args);

            VERIFY_IS_NOT_NULL(source);

            std::cout << "Enumeration Completed " << std::endl;

            allEnumerationCompleted.SetEvent();
        });

    auto cleanup = wil::scope_exit([&]
        {
            std::cout << "Cleaning up watcher" << std::endl;

            if (watcher == nullptr) return;

            watcher.Stop();

            // cleanup

            if (addedToken) watcher.Added(addedToken);
            if (removedToken) watcher.Removed(removedToken);
            if (updatedToken) watcher.Updated(updatedToken);
            if (enumerationCompletedToken) watcher.EnumerationCompleted(enumerationCompletedToken);

        });

    watcher.Start();


    // Wait for incoming message
    if (!allEnumerationCompleted.wait(10000))
    {
        std::cout << "Failure waiting for enumeration complete, timed out." << std::endl;
        VERIFY_FAIL();
    }

    // make sure we collected all the ports correctly
    VERIFY_ARE_EQUAL(watcher.EnumeratedPorts().Size(), countAdded - countRemoved);
    VERIFY_ARE_EQUAL(watcher.EnumeratedPorts().Size(), watcher.CountSourcePorts() + watcher.CountDestinationPorts());

}


void MidiLegacyPortDeviceWatcherTests::TestGetMethods()
{

    wil::unique_event_nothrow allEnumerationCompleted;
    allEnumerationCompleted.create();

    auto watcher = MidiLegacyPortDeviceWatcher::Create();
    VERIFY_IS_NOT_NULL(watcher);

    auto enumerationCompletedToken = watcher.EnumerationCompleted([&](auto const& source, auto const& args)
        {
            // args are null as is typical for a watcher Enumeration Completed, Started, or Stopped event.
            UNREFERENCED_PARAMETER(args);

            VERIFY_IS_NOT_NULL(source);

            std::cout << "Enumeration Completed " << std::endl;

            allEnumerationCompleted.SetEvent();
        });

    auto cleanup = wil::scope_exit([&]
        {
            std::cout << "Cleaning up watcher" << std::endl;

            if (watcher == nullptr) return;

            watcher.Stop();

            // cleanup

            if (enumerationCompletedToken) watcher.EnumerationCompleted(enumerationCompletedToken);

        });

    watcher.Start();

    // Wait for incoming message
    if (!allEnumerationCompleted.wait(10000))
    {
        std::cout << "Failure waiting for enumeration complete, timed out." << std::endl;
        VERIFY_FAIL();
    }

    // now that we have all the devices, do some verification on the Get methods.

    auto numDestinations = midiOutGetNumDevs();
    auto numSources = midiInGetNumDevs();   

    auto allSources = watcher.GetEnumeratedPortsForFlow(Midi1PortFlow::MidiMessageSource);
    auto allDestinations = watcher.GetEnumeratedPortsForFlow(Midi1PortFlow::MidiMessageDestination);

    // because of .drv-based ports, these numbers are not going to be equal in cases
    //VERIFY_ARE_EQUAL(numDestinations, allDestinations.Size());
    //VERIFY_ARE_EQUAL(numSources, allSources.Size());

    std::cout << "Verifying sources by port number." << std::endl;

    for (uint32_t i = 0; i < numSources; i++)
    {
        auto port = watcher.GetEnumeratedPortForNumber(i, Midi1PortFlow::MidiMessageSource);
        
        if (port == nullptr)
        {
            std::cout << "No port found for source number " << i << std::endl;
            continue;
        }

        VERIFY_ARE_EQUAL(port.Number(), i);
    }

    std::cout << "Verifying destinations by port number." << std::endl;

    for (uint32_t i = 0; i < numDestinations; i++)
    {
        auto port = watcher.GetEnumeratedPortForNumber(i, Midi1PortFlow::MidiMessageDestination);

        if (port == nullptr)
        {
            std::cout << "No port found for destination number " << i << std::endl;
            continue;
        }

        VERIFY_ARE_EQUAL(port.Number(), i);
    }



}

