// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"



void MidiLoopbackEndpointTests::TestCreateLoopback()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointAId{};
    winrt::hstring endpointBId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A", // name
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        uniqueId, // can be the same as the first one, but doesn't need to be.
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
        endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();


        std::cout
            << "Loopback Endpoint A: " << std::endl
            << " - " << winrt::to_string(endpointAId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().EndpointA().Name())
            << std::endl << std::endl;

        std::cout
            << "Loopback Endpoint B: " << std::endl
            << " - " << winrt::to_string(endpointBId)
            << " - " << winrt::to_string(response.CreatedLoopbackEntry().EndpointB().Name())
            << std::endl << std::endl;


        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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

void MidiLoopbackEndpointTests::TestCreateLegacyPorts()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointAId{};
    winrt::hstring endpointBId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A", // name
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        uniqueId, // can be the same as the first one, but doesn't need to be.
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
        endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();

        auto endpointAPorts = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointAId);
        VERIFY_IS_NOT_NULL(endpointAPorts);
        VERIFY_IS_TRUE(endpointAPorts.Size() > 0);

        std::cout << std::endl;
        for (auto const& portInfo : endpointAPorts)
        {
            if (portInfo.Flow() == Midi1PortFlow::MidiMessageDestination)
            {
                std::cout << "Destination Port for Endpoint A: ";
            }
            else
            {
                std::cout << "Source Port for Endpoint A: ";
            }

            std::cout
                << " - " << winrt::to_string(portInfo.PortDeviceInstanceId())
                << " - " << winrt::to_string(portInfo.Name())
                << std::endl;
        }

        auto endpointBPorts = MidiLegacyPortDeviceInformation::FindAllForAssociatedEndpoint(endpointBId);
        VERIFY_IS_NOT_NULL(endpointBPorts);
        VERIFY_IS_TRUE(endpointBPorts.Size() > 0);

        std::cout << std::endl;
        for (auto const& portInfo : endpointBPorts)
        {
            if (portInfo.Flow() == Midi1PortFlow::MidiMessageDestination)
            {
                std::cout << "Destination Port for Endpoint B: ";
            }
            else
            {
                std::cout << "Source Port for Endpoint B: ";
            }

            std::cout
                << " - " << winrt::to_string(portInfo.PortDeviceInstanceId())
                << " - " << winrt::to_string(portInfo.Name())
                << std::endl;
        }



        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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



void MidiLoopbackEndpointTests::TestUmpSendReceive()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());
    VERIFY_IS_TRUE(MidiLoopbackManager::IsTransportAvailable());

    winrt::hstring endpointAId{};
    winrt::hstring endpointBId{};

    auto uniqueId = L"ID" + winrt::to_hstring(MidiClock::Now());

    // A-side of the loopback
    MidiLoopbackEndpointDefinition definitionA(
        L"Test Loopback A", // name
        uniqueId, // unique Id that identifies the loopback
        L"The first description is optional, but is displayed to users. This becomes the transport-defined description." // description
    );

    // B-side of the loopback
    MidiLoopbackEndpointDefinition definitionB(
        L"Test Loopback B",
        uniqueId, // can be the same as the first one, but doesn't need to be.
        L"The second description is optional, but is displayed to users. This becomes the transport-defined description."
    );

    winrt::guid associationId = foundation::GuidHelper::CreateNewGuid();

    LOG_OUTPUT(L"Creating loopback endpoint creation config");

    MidiLoopbackCreationConfig creationConfig(associationId, definitionA, definitionB);

    LOG_OUTPUT(L"Creating loopbacks");

    auto response = MidiLoopbackManager::CreateTransientLoopback(creationConfig);
    VERIFY_IS_NOT_NULL(response);

    if (response.Success())
    {
        LOG_OUTPUT(L"Endpoints created successfully");

        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointA());
        VERIFY_IS_NOT_NULL(response.CreatedLoopbackEntry().EndpointB());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId().empty());
        VERIFY_IS_FALSE(response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId().empty());

        endpointAId = response.CreatedLoopbackEntry().EndpointA().EndpointDeviceId();
        endpointBId = response.CreatedLoopbackEntry().EndpointB().EndpointDeviceId();


        std::cout << "Creating messages" << std::endl;

        MidiMessage32 messageAToB;
        messageAToB.Timestamp(MidiClock::TimestampConstantSendImmediately());
        messageAToB.Word0(0x23001627);

        MidiMessage64 messageBToA;
        messageBToA.Timestamp(MidiClock::TimestampConstantSendImmediately());
        messageBToA.Word0(0x43001627);
        messageBToA.Word1(0x86753090);

        bool messageAToBReceived{ false };
        bool messageBToAReceived{ false };

        std::cout << "Setting up events" << std::endl;
        wil::unique_event_nothrow allMessagesReceived;
        allMessagesReceived.create();


        std::cout << "Creating session" << std::endl;

        auto session = MidiSession::Create(L"TAEF TestLoopbackEndpointConnections Session");
        VERIFY_IS_NOT_NULL(session);


        std::cout << "Creating connections" << std::endl;

        auto connectionA = session.CreateEndpointConnection(endpointAId);
        VERIFY_IS_NOT_NULL(connectionA);

        auto connectionB = session.CreateEndpointConnection(endpointBId);
        VERIFY_IS_NOT_NULL(connectionB);

        connectionA.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(args);
                VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket64);

                auto message = args.GetMessagePacket().as<MidiMessage64>();

                VERIFY_ARE_EQUAL(message.Word0(), messageBToA.Word0());
                VERIFY_ARE_EQUAL(message.Word1(), messageBToA.Word1());

                messageBToAReceived = true;
                if (messageAToBReceived && messageBToAReceived)
                {
                    allMessagesReceived.SetEvent();
                }
            });

        connectionB.MessageReceived([&](auto&&, MidiMessageReceivedEventArgs const& args)
            {
                VERIFY_IS_NOT_NULL(args);
                VERIFY_IS_TRUE(args.PacketType() == MidiPacketType::UniversalMidiPacket32);

                auto message = args.GetMessagePacket().as<MidiMessage32>();

                VERIFY_ARE_EQUAL(message.Word0(), messageAToB.Word0());

                messageAToBReceived = true;
                if (messageAToBReceived && messageBToAReceived)
                {
                    allMessagesReceived.SetEvent();
                }
            });

        std::cout << "Opening connections" << std::endl;

        VERIFY_IS_TRUE(connectionA.Open());
        VERIFY_IS_TRUE(connectionB.Open());

        // send messages
        std::cout << "Sending messages" << std::endl;
        connectionA.SendSingleMessagePacket(messageAToB);
        connectionB.SendSingleMessagePacket(messageBToA);

        std::cout << "Waiting..." << std::endl;
        allMessagesReceived.wait(5000);

        VERIFY_IS_TRUE(messageAToBReceived);
        VERIFY_IS_TRUE(messageBToAReceived);





        // Give a hoot. Don't pollute.
        MidiLoopbackRemovalConfig removalConfig(response.CreatedLoopbackEntry().AssociationId());
        auto removalResponse = MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

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