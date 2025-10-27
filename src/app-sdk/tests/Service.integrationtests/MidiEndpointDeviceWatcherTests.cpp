// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


_Use_decl_annotations_
void MidiEndpointDeviceWatcherTests::TestWatcherEnumeration(MidiEndpointDeviceInformationFilters filter, uint32_t numEndpointsExpected)
{
    auto initializer = InitWinRTAndSDK_MTA();

    auto cleanup = wil::scope_exit([&]
        {
            ShutdownSDKAndWinRT(initializer);
        });


    wil::unique_event_nothrow enumerationCompleted;
    enumerationCompleted.create();

    auto watcher = MidiEndpointDeviceWatcher::Create(filter);
    VERIFY_IS_NOT_NULL(watcher);

    // we're enumerating only the loopbacks, so that should be 2 endpoints total
    uint32_t numEndpointsActual{ 0 };

    bool completedEventFired{ false };

    auto EnumerationCompletedHandler = [&](MidiEndpointDeviceWatcher const& /*sender*/, winrt::Windows::Foundation::IInspectable const& /*args*/)
        {
            std::cout << "Enumeration completed event raised." << std::endl;

            completedEventFired = true;

            enumerationCompleted.SetEvent();
        };

    auto enumerationCompletedEventRevokeToken = watcher.EnumerationCompleted(EnumerationCompletedHandler);


    auto EndpointDeviceAddedHandler = [&](MidiEndpointDeviceWatcher const& /*sender*/, MidiEndpointDeviceInformationAddedEventArgs const& args)
        {
            std::cout << "Device Added event raised." << std::endl;
            std::cout << "Id:             " << winrt::to_string(args.AddedDevice().EndpointDeviceId()) << std::endl;
            std::cout << "Name:           " << winrt::to_string(args.AddedDevice().Name()) << std::endl;
            //std::cout << "Transport Desc: " << winrt::to_string(args.AddedDevice().TransportSuppliedDescription()) << std::endl;
            //std::cout << "User Desc:      " << winrt::to_string(args.AddedDevice().UserSuppliedDescription()) << std::endl;
            std::cout << std::endl;

            numEndpointsActual++;
        };

    auto addedEventRevokeToken = watcher.Added(EndpointDeviceAddedHandler);


    watcher.Start();

    if (!enumerationCompleted.wait(10000))
    {
        std::cout << "Failure waiting for enumeration to complete, timed out." << std::endl;
    }

    // verify that enumeration actually completed
    VERIFY_IS_TRUE(completedEventFired);

    VERIFY_ARE_EQUAL(numEndpointsActual, numEndpointsExpected);

    watcher.Stop();


    // unwire events
    watcher.EnumerationCompleted(enumerationCompletedEventRevokeToken);
    watcher.Added(addedEventRevokeToken);

    // if you really want to call uninit_apartment, you must release all your COM and WinRT references first
    // these don't go out of scope here and self-destruct, so we set them to nullptr
    watcher = nullptr;

}



void MidiEndpointDeviceWatcherTests::TestWatcherEnumerationAllDiagnosticsEndpoints()
{
    TestWatcherEnumeration(
        MidiEndpointDeviceInformationFilters::DiagnosticLoopback | MidiEndpointDeviceInformationFilters::DiagnosticPing,
        3);
}

void MidiEndpointDeviceWatcherTests::TestWatcherEnumerationPingEndpoint()
{
    TestWatcherEnumeration(
        MidiEndpointDeviceInformationFilters::DiagnosticPing, 
        1);

}

void MidiEndpointDeviceWatcherTests::TestWatcherEnumerationLoopbackEndpoints()
{
    TestWatcherEnumeration(
        MidiEndpointDeviceInformationFilters::DiagnosticLoopback,
        2);
}


// can't really test device plug/unplug or update here because it's interactive and this test is run automated