// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <algorithm>

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



    // Test GetEnumeratedPortForNumber

    std::cout << "GetEnumeratedPortForNumber: Verifying sources by port number." << std::endl;

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

    std::cout << "GetEnumeratedPortForNumber: Verifying destinations by port number." << std::endl;

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


    // Test GetEnumeratedPortsForFlow

    for (auto const& flow : { Midi1PortFlow::MidiMessageSource, Midi1PortFlow::MidiMessageDestination })
    {
        std::cout << "GetEnumeratedPortsForFlow: " << (flow == Midi1PortFlow::MidiMessageSource ? "Source" : "Destination")  << std::endl;

        auto ports = watcher.GetEnumeratedPortsForFlow(flow);
        VERIFY_IS_NOT_NULL(ports);
        VERIFY_IS_TRUE(ports.Size() > 0);
    }

    // these next tests verify against the list of all ports. We 
    // get it a slightly different way just to validate. We could also
    // get it using midiIn/OutGetDevCaps but the .drv-based ports mess
    // this up. So instead, we check for internal consistency.

    auto allPorts = MidiLegacyPortDeviceInformation::FindAll();
    VERIFY_IS_NOT_NULL(allPorts);
    VERIFY_IS_TRUE(allPorts.Size() > 0);

    uint32_t missingDeviceInterfaceIdCount{ 0 };

    for (auto const& port : allPorts)
    {
        std::wcout << L"Verifying Get methods for port: " << port.Name().c_str() << L", Number: " << port.Number() << std::endl;

        // Test GetEnumeratedPortsForName
        VERIFY_IS_FALSE(port.Name().empty());
        auto foundPortsByName = watcher.GetEnumeratedPortsForName(port.Name());
        VERIFY_IS_NOT_NULL(foundPortsByName);
        VERIFY_IS_TRUE(foundPortsByName.Size() > 0);

        // Test GetEnumeratedPortsForParent
        VERIFY_IS_FALSE(port.ParentDeviceInstanceId().empty());
        auto foundPortsByParent = watcher.GetEnumeratedPortsForParent(port.ParentDeviceInstanceId());
        VERIFY_IS_NOT_NULL(foundPortsByParent);
        VERIFY_IS_TRUE(foundPortsByParent.Size() > 0);

        // Test GetEnumeratedPortsForDriverDeviceInterfaceId

        if (port.DriverDeviceInterfaceId().empty())
        {
            // this is valid for the GS wavetable synth and likely for shoe-horned replacements for it
            // so don't want to check for specific strings in the id or anything. 
            std::wcout << L" *** Port " << port.Name().c_str() << L" has empty DriverDeviceInterfaceId, skipping GetEnumeratedPortsForDriverDeviceInterfaceId test." << std::endl;
            missingDeviceInterfaceIdCount++;
            continue;
        }

        auto foundPortsByDriverDeviceInterfaceId = watcher.GetEnumeratedPortsForDriverDeviceInterfaceId(port.DriverDeviceInterfaceId());
        VERIFY_IS_NOT_NULL(foundPortsByDriverDeviceInterfaceId);
        VERIFY_IS_TRUE(foundPortsByDriverDeviceInterfaceId.Size() > 0);



        // Test GetEnumeratedPortsForAssociatedEndpoint



        // Test GetEnumeratedPortsForAssociatedEndpointAndGroup



    }

    // at least some ports should have a driver device interface id to be valid.
    VERIFY_IS_TRUE(missingDeviceInterfaceIdCount < allPorts.Size()); 

}


// ============================================================================
// Claude Opus 5 authored
//
// Verifies that the watcher raises Added for every MIDI 1.0 port created while
// it is running, and that each port carries its associated endpoint device id.
//
// Creating a transient A/B loopback creates four ports at once: a source and a
// destination for each of the A and B endpoints.
//
// NOTE ON TIMING: this test waits for EnumerationCompleted before creating the
// loopback, and that wait is load bearing.
//
// A Windows.Devices.Enumeration DeviceWatcher does not surface newly arrived
// devices until its initial enumeration has finished. That enumeration has to
// evaluate every registered MIDI 1.0 port device interface, including ones which
// are no longer active. Because the port device interfaces belonging to transient
// loopbacks are not removed from the device tree when the loopback is removed,
// they accumulate over time (this machine had 922 registered, 756 of which were
// leftover loopback ports, against roughly 63 active ports). The more that have
// accumulated, the longer the initial enumeration takes.
//
// Creating ports before EnumerationCompleted therefore results in some or all of
// them never being reported to the application. Restarting midisrv clears the
// backlog and masks the problem.
// ============================================================================
void MidiLegacyPortDeviceWatcherTests::TestAddedRaisedForPortsCreatedWhileWatching()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    wil::critical_section addedLock;
    wil::unique_event_nothrow enumerationCompleted;
    enumerationCompleted.create();

    // port device id -> associated endpoint device id, as reported through Added
    std::map<std::wstring, std::wstring> addedPorts;

    auto watcher = MidiLegacyPortDeviceWatcher::Create();
    VERIFY_IS_NOT_NULL(watcher);

    auto addedToken = watcher.Added([&](auto const&, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            // Deliberately no TAEF VERIFY calls in here. This handler is invoked
            // concurrently on watcher threads while the initial enumeration is still
            // running, and we only want to record what we were given.
            if (args == nullptr) return;

            auto port = args.AddedDevice();

            if (port == nullptr) return;

            auto lock = addedLock.lock();

            addedPorts.insert_or_assign(
                std::wstring(port.PortDeviceId().c_str()),
                std::wstring(port.AssociatedEndpointDeviceId().c_str()));


            if (port.TransportId() == MidiLoopbackManager::TransportId())
            {
                std::wcout
                    << L"Added: " << port.Name().c_str() << std::endl
                    << L" - PortDeviceId: " << port.PortDeviceId().c_str() << std::endl
                    << L" - AssociatedEndpointDeviceId: " << port.AssociatedEndpointDeviceId().c_str() << std::endl
                    << std::endl;
            }
        });

    auto enumerationCompletedToken = watcher.EnumerationCompleted([&](auto const&, auto const&)
        {
            enumerationCompleted.SetEvent();
        });

    auto cleanupWatcher = wil::scope_exit([&]
        {
            if (watcher == nullptr) return;

            watcher.Stop();

            if (addedToken) watcher.Added(addedToken);
            if (enumerationCompletedToken) watcher.EnumerationCompleted(enumerationCompletedToken);
        });

    LOG_OUTPUT(L"Starting watcher and waiting for the initial enumeration to complete");

    watcher.Start();

    // Wait for the initial enumeration to complete before creating the ports.
    //
    // This matters: a Windows.Devices.Enumeration DeviceWatcher does not surface
    // newly arrived devices until its initial enumeration has finished. On a machine
    // with a large number of registered MIDI 1.0 port device interfaces that
    // enumeration can take many seconds, so ports created before EnumerationCompleted
    // may be reported late or, from the application's point of view, not at all.
    VERIFY_IS_TRUE(enumerationCompleted.wait(30000));

    // Create an A/B loopback. This creates four MIDI 1.0 ports at once.
    auto uniqueId = winrt::to_hstring(foundation::GuidHelper::CreateNewGuid());

    MidiLoopbackEndpointDefinition definitionA(
        L"Test Watcher Added Repro A",
        uniqueId + L"-A",
        L"A-side of the loopback used to repro the watcher Added defect.");

    MidiLoopbackEndpointDefinition definitionB(
        L"Test Watcher Added Repro B",
        uniqueId + L"-B",
        L"B-side of the loopback used to repro the watcher Added defect.");

    MidiLoopbackCreationConfig creationConfig(
        foundation::GuidHelper::CreateNewGuid(), definitionA, definitionB);

    LOG_OUTPUT(L"Creating the A/B loopback (creates four MIDI 1.0 ports at once)");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);
    VERIFY_IS_TRUE(response.Success());

    auto associationId = response.CreatedLoopbackEntry().AssociationId();
    auto endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    auto endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

    auto cleanupLoopback = wil::scope_exit([&]
        {
            MidiLoopbackRemovalConfig removalConfig(associationId);
            auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

            VERIFY_IS_NOT_NULL(removalResponse);
            VERIFY_IS_TRUE(removalResponse.Success());
        });

    // Give the watcher a generous amount of time to report all four ports.
    LOG_OUTPUT(L"Waiting for the watcher to report the new ports");
    Sleep(15000);


    // this claude code crashes the test with an exception, so I commented it out
    //std::wcout << L"Initial enumeration completed during the wait: "
    //           << (enumerationCompleted.is_signaled() ? L"yes" : L"no") << std::endl;


    // Ground truth: query the ports for both endpoints directly. Each port device
    // id is expected to map to the endpoint it was created under.
    std::map<std::wstring, std::wstring> expectedPorts;

    for (auto const& endpointId : { endpointAId, endpointBId })
    {
        auto ports = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointId);
        VERIFY_IS_NOT_NULL(ports);

        for (auto const& port : ports)
        {
            std::wcout
                << L"Expecting (found via non-watcher enumeration call): " << port.Name().c_str() << std::endl
                << L" - PortDeviceId: " << port.PortDeviceId().c_str() << std::endl
                << L" - AssociatedEndpointDeviceId: " << port.AssociatedEndpointDeviceId().c_str() << std::endl
                << std::endl;


            expectedPorts.insert_or_assign(
                std::wstring(port.PortDeviceId().c_str()),
                std::wstring(port.AssociatedEndpointDeviceId().c_str()));
        }
    }

    // an A/B loopback has a source and a destination for each of its two endpoints
    std::wcout << L"Ports found by direct query: " << expectedPorts.size() << std::endl;
    VERIFY_ARE_EQUAL(expectedPorts.size(), (size_t)4);

    size_t reportedByWatcherCount{ 0 };
    size_t withCorrectAssociatedEndpointCount{ 0 };

    {
        auto lock = addedLock.lock();

        for (auto const& expected : expectedPorts)
        {
            auto const& expectedPortDeviceId = expected.first;
            auto const& expectedAssociatedEndpointId = expected.second;

            auto found = addedPorts.find(expectedPortDeviceId);

            if (found == addedPorts.end())
            {
                std::wcout << L"  NOT REPORTED BY WATCHER : " << expectedPortDeviceId << std::endl;
                continue;
            }

            reportedByWatcherCount++;

            auto const& actualAssociatedEndpointId = found->second;

            if (_wcsicmp(actualAssociatedEndpointId.c_str(), expectedAssociatedEndpointId.c_str()) == 0)
            {
                withCorrectAssociatedEndpointCount++;

                std::wcout << L"  OK                      : " << expectedPortDeviceId << std::endl;
            }
            else
            {
                std::wcout
                    << L"  WRONG ASSOCIATED ENDPOINT: " << expectedPortDeviceId << std::endl
                    << L"      expected : '" << expectedAssociatedEndpointId << L"'" << std::endl
                    << L"      from Added: '" << actualAssociatedEndpointId << L"'" << std::endl;
            }
        }
    }

    std::wcout << L"Reported by watcher:                      " << reportedByWatcherCount << std::endl;
    std::wcout << L"With correct AssociatedEndpointDeviceId:  " << withCorrectAssociatedEndpointCount << std::endl;

    // The watcher must have raised Added for all four of the newly created ports,
    // each carrying the endpoint it belongs to.
    VERIFY_ARE_EQUAL(reportedByWatcherCount, expectedPorts.size());
    VERIFY_ARE_EQUAL(withCorrectAssociatedEndpointCount, expectedPorts.size());

}

