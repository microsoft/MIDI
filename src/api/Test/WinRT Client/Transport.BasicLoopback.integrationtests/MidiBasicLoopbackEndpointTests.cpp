// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"

#include <mmsystem.h>
#include <vector>

#pragma comment(lib, "winmm.lib")


void MidiBasicLoopbackTests::TestReopenLegacyWinMMPorts()
{
    // Regression test for issue 1070: after opening and closing the WinMM
    // ports created for a basic loopback, we must be able to re-open them.

    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    // Start the legacy port device watcher *before* creating the loopback so
    // we receive the Added events for the newly created ports.
    LOG_OUTPUT(L"Creating and starting the legacy port device watcher");

    auto watcher = MidiLegacyPortDeviceWatcher::Create();
    VERIFY_IS_NOT_NULL(watcher);

    wil::critical_section portListLock;
    wil::unique_event_nothrow bothPortsAvailable;
    bothPortsAvailable.create();

    winrt::hstring endpointId{};

    uint32_t sourcePortNumber{ 0 };
    uint32_t destinationPortNumber{ 0 };
    bool haveSourcePort{ false };
    bool haveDestinationPort{ false };

    std::vector<MidiLegacyPortDeviceInformation> addedDevices;

    // Evaluate the ports we've collected so far against our loopback endpoint.
    // Must be called while holding portListLock.
    auto evaluateAddedDevices = [&]()
        {
            if (endpointId.empty())
            {
                return;
            }

            for (auto const& device : addedDevices)
            {
                if (_wcsicmp(device.AssociatedEndpointDeviceId().c_str(), endpointId.c_str()) != 0)
                {
                    continue;
                }

                if (device.Flow() == Midi1PortFlow::MidiMessageSource && !haveSourcePort)
                {
                    sourcePortNumber = device.Number();
                    haveSourcePort = true;
                }
                else if (device.Flow() == Midi1PortFlow::MidiMessageDestination && !haveDestinationPort)
                {
                    destinationPortNumber = device.Number();
                    haveDestinationPort = true;
                }
            }

            if (haveSourcePort && haveDestinationPort)
            {
                bothPortsAvailable.SetEvent();
            }
        };

    auto addedToken = watcher.Added([&](auto const& /*source*/, MidiLegacyPortDeviceInformationAddedEventArgs const& args)
        {
            auto lock = portListLock.lock();

            addedDevices.push_back(args.AddedDevice());
            evaluateAddedDevices();
        });

    watcher.Start();

    // Create the basic loopback

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback GH1070",
        uniqueId,
        L"Regression test loopback for issue GH1070."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");
    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopback");
    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);
    VERIFY_IS_TRUE(response.Success());
    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

    auto removalAssociationId = response.CreatedLoopbackEntry().AssociationId();

    // Ensure we always remove the loopback we created.
    auto cleanupLoopback = wil::scope_exit([&]
        {
            LOG_OUTPUT(L"Removing loopback");
            MidiBasicLoopbackRemovalConfig removalConfig(removalAssociationId);
            auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

            VERIFY_IS_NOT_NULL(removalResponse);
            VERIFY_IS_TRUE(removalResponse.Success());
        });

    {
        auto lock = portListLock.lock();
        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        // The ports may already have been reported to the watcher before we knew
        // the endpoint id, so re-evaluate what we've collected so far.
        evaluateAddedDevices();
    }

    LOG_OUTPUT(L"Waiting for the source and destination legacy ports to appear in the watcher");
    if (!bothPortsAvailable.wait(10000))
    {
        LOG_OUTPUT(L"Timed out waiting for the legacy ports to be added.");
        VERIFY_FAIL();
        return;
    }


    // stop watching before we do anything else
    if (watcher != nullptr)
    {
        if (addedToken) watcher.Added(addedToken);

        watcher.Stop();
    }

    VERIFY_IS_TRUE(haveSourcePort);
    VERIFY_IS_TRUE(haveDestinationPort);

    LOG_OUTPUT(WEX::Common::String().Format(
        L"Source WinMM port number: %u, Destination WinMM port number: %u",
        sourcePortNumber, destinationPortNumber));

    // Open and close the WinMM ports several times in a row, verifying that the
    // ports can be re-opened each time (this is the core of issue 1070).
    const int iterations = 4;

    midiInGetNumDevs(); // no longer required after June 2026 CFR
    midiOutGetNumDevs(); // no longer required after June 2026 CFR

    for (int i = 0; i < iterations; i++)
    {
        LOG_OUTPUT(WEX::Common::String().Format(L"WinMM open/close iteration %d of %d", i + 1, iterations));

        // A MIDI message "source" is a WinMM MIDI input port.
        HMIDIIN hMidiIn{ nullptr };
        auto inResult = midiInOpen(&hMidiIn, sourcePortNumber, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(inResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));
        VERIFY_IS_NOT_NULL(hMidiIn);

        // A MIDI message "destination" is a WinMM MIDI output port.
        HMIDIOUT hMidiOut{ nullptr };
        auto outResult = midiOutOpen(&hMidiOut, destinationPortNumber, 0, 0, CALLBACK_NULL);
        VERIFY_ARE_EQUAL(outResult, static_cast<MMRESULT>(MMSYSERR_NOERROR));
        VERIFY_IS_NOT_NULL(hMidiOut);

        // Close the WinMM ports
        if (inResult == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiInClose(hMidiIn), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        if (outResult == MMSYSERR_NOERROR)
        {
            VERIFY_ARE_EQUAL(midiOutClose(hMidiOut), static_cast<MMRESULT>(MMSYSERR_NOERROR));
        }

        // Give a moment before attempting to re-open
        Sleep(100);
    }

    // cleanupLoopback scope_exit handler removes the loopback
}


void MidiBasicLoopbackTests::TestCreateLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback", // name
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        std::cout
            << "Loopback Endpoint: " << std::endl
            << " - " << winrt::to_string(endpointId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().Name())
            << std::endl << std::endl;


        // Call GetActiveLoopbackEntries and validate that the new entry is present in the list

        auto entries = MidiBasicLoopbackManager::GetActiveLoopbackEntries();
        bool thisLoopbackEntryFound = false;

        for (auto const& entry : entries)
        {
            if (entry.AssociationId() == response.CreatedLoopbackEntry().AssociationId())
            {
                thisLoopbackEntryFound = true;
                break;
            }
        }

        VERIFY_IS_TRUE(thisLoopbackEntryFound);

        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());

    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }
}

void MidiBasicLoopbackTests::TestCreateLegacyPorts()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback", // name
        uniqueId, // unique Id that identifies the loopback
        L"The description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        std::cout
            << "Loopback Endpoint: " << std::endl
            << " - " << winrt::to_string(endpointId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().Name())
            << std::endl << std::endl;

        auto endpointPorts = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointId);
        VERIFY_IS_NOT_NULL(endpointPorts);
        VERIFY_IS_TRUE(endpointPorts.Size() > 0);

        std::cout << std::endl;
        for (auto const& portInfo : endpointPorts)
        {
            if (portInfo.Flow() == Midi1PortFlow::MidiMessageDestination)
            {
                std::cout << "Destination Port for Endpoint: ";
            }
            else
            {
                std::cout << "Source Port for Endpoint: ";
            }

            std::cout
                << " - " << winrt::to_string(portInfo.AssociatedEndpointDeviceId())
                << " - " << winrt::to_string(portInfo.Name())
                << std::endl;
        }


        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());

    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }
}



void MidiBasicLoopbackTests::TestUmpSendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    VERIFY_IS_TRUE(MidiBasicLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    MidiBasicLoopbackEndpointDefinition definition(
        L"Test Basic Loopback", // name
        uniqueId, // unique Id that identifies the loopback
        L"The description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiBasicLoopbackCreationConfig creationConfig(associationId, definition);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointDeviceId().empty());

        endpointId = response.CreatedLoopbackEntry().EndpointDeviceId();

        std::cout
            << "Loopback Endpoint: " << std::endl
            << " - " << winrt::to_string(endpointId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().Name())
            << std::endl << std::endl;


        std::cout << "Setting up events" << std::endl;
        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();

        bool messageReceived { false };

        std::cout << "Creating messages" << std::endl;

        MidiMessage64 message;
        message.Timestamp(MidiClock::TimestampConstantSendImmediately());
        message.Word0(0x43001627);
        message.Word1(0x86753090);

        std::cout << "Creating session" << std::endl;

        auto session = MidiSession::Create(L"TAEF TestBasicLoopbackEndpointConnections Session");
        VERIFY_IS_NOT_NULL(session);

        std::cout << "Creating connections" << std::endl;

        auto connection = session.CreateEndpointConnection(endpointId);
        VERIFY_IS_NOT_NULL(connection);

        connection.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(args);
                VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket64);

                auto message = args.GetMessagePacket().as<MidiMessage64>();

                VERIFY_ARE_EQUAL(message.Word0(), message.Word0());

                messageReceived = true;
                allMessagesReceived.SetEvent();
            });

        std::cout << "Opening connections" << std::endl;

        VERIFY_IS_TRUE(connection.Open());

        // send messages
        std::cout << "Sending messages" << std::endl;
        connection.SendSingleMessagePacket(message);

        std::cout << "Waiting..." << std::endl;
        allMessagesReceived.wait(5000);

        VERIFY_IS_TRUE(messageReceived);




        // Give a hoot. Don't pollute.
        MidiBasicLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

        VERIFY_IS_NOT_NULL(removalResponse);
        VERIFY_IS_TRUE(removalResponse.Success());

    }
    else
    {
        LOG_OUTPUT(L"Return result indicates failure");

        std::wcout << L"Success:       " << response.Success() << std::endl;
        std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
        std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

        VERIFY_FAIL();
    }






    ////    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    //VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    //winrt::hstring endpointAId{};
    //winrt::hstring endpointBId{};

    //auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    //// A-side of the loopback
    //MidiLoopbackEndpointDefinition definitionA(
    //    L"Test Loopback A", // name
    //    uniqueId, // unique Id that identifies the loopback
    //    L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    //);

    //// B-side of the loopback
    //MidiLoopbackEndpointDefinition definitionB(
    //    L"Test Loopback B",
    //    uniqueId, // can be the same as the first one, but doesn't need to be.
    //    L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    //);

    //winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    //LOG_OUTPUT(L"Creating loopback endpoint creation config");

    //MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

    //LOG_OUTPUT(L"Creating loopbacks");

    //auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    //VERIFY_IS_NOT_NULL(response);

    //if (response.Success())
    //{
    //    LOG_OUTPUT(L"Endpoints created successfully");

    //    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
    //    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
    //    VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
    //    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
    //    VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

    //    endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
    //    endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();


    //    std::cout << "Creating messages" << std::endl;

    //    MidiMessage32 messageAToB;
    //    messageAToB.Timestamp(MidiClock::TimestampConstantSendImmediately());
    //    messageAToB.Word0(0x23001627);

    //    MidiMessage64 messageBToA;
    //    messageBToA.Timestamp(MidiClock::TimestampConstantSendImmediately());
    //    messageBToA.Word0(0x43001627);
    //    messageBToA.Word1(0x86753090);

    //    bool messageAToBReceived{ false };
    //    bool messageBToAReceived{ false };

    //    std::cout << "Setting up events" << std::endl;
    //    wil::unique_event_nothrow allMessagesReceived;
    //    allMessagesReceived.create();


    //    std::cout << "Creating session" << std::endl;

    //    auto session = MidiSession::Create(L"TAEF TestLoopbackEndpointConnections Session");
    //    VERIFY_IS_NOT_NULL(session);


    //    std::cout << "Creating connections" << std::endl;

    //    auto connectionA = session.CreateEndpointConnection(endpointAId);
    //    VERIFY_IS_NOT_NULL(connectionA);

    //    auto connectionB = session.CreateEndpointConnection(endpointBId);
    //    VERIFY_IS_NOT_NULL(connectionB);

    //    connectionA.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
    //        {
    //            VERIFY_IS_NOT_NULL(args);
    //            VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket64);

    //            auto message = args.GetMessagePacket().as<MidiMessage64>();

    //            VERIFY_ARE_EQUAL(message.Word0(), messageBToA.Word0());
    //            VERIFY_ARE_EQUAL(message.Word1(), messageBToA.Word1());

    //            messageBToAReceived = true;
    //            if (messageAToBReceived && messageBToAReceived)
    //            {
    //                allMessagesReceived.SetEvent();
    //            }
    //        });

    //    connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
    //        {
    //            VERIFY_IS_NOT_NULL(args);
    //            VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket32);

    //            auto message = args.GetMessagePacket().as<MidiMessage32>();

    //            VERIFY_ARE_EQUAL(message.Word0(), messageAToB.Word0());

    //            messageAToBReceived = true;
    //            if (messageAToBReceived && messageBToAReceived)
    //            {
    //                allMessagesReceived.SetEvent();
    //            }
    //        });

    //    std::cout << "Opening connections" << std::endl;

    //    VERIFY_IS_TRUE(connectionA.Open());
    //    VERIFY_IS_TRUE(connectionB.Open());

    //    // send messages
    //    std::cout << "Sending messages" << std::endl;
    //    connectionA.SendSingleMessagePacket(messageAToB);
    //    connectionB.SendSingleMessagePacket(messageBToA);

    //    std::cout << "Waiting..." << std::endl;
    //    allMessagesReceived.wait(5000);

    //    VERIFY_IS_TRUE(messageAToBReceived);
    //    VERIFY_IS_TRUE(messageBToAReceived);





    //    // Give a hoot. Don't pollute.
    //    MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
    //    auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

    //    VERIFY_IS_NOT_NULL(removalResponse);
    //    VERIFY_IS_TRUE(removalResponse.Success());

    //}
    //else
    //{
    //    LOG_OUTPUT(L"Return result indicates failure");

    //    std::wcout << L"Success:       " << response.Success() << std::endl;
    //    std::wcout << L"Error Code:    " << std::hex << static_cast<uint32_t>(response.ErrorCode()) << std::dec << std::endl;
    //    std::wcout << L"Error Message: " << response.ErrorMessage().c_str() << std::endl;

    //    VERIFY_FAIL();
    //}
}
