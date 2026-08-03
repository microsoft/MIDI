// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"



void MidiDiagnosticLoopbackEndpointTests::TestLoopbackEndpointConnections()
{
    VERIFY_IS_TRUE(MidiApi::EnsureServiceAvailable());

    std::cout << "Testing loopback endpoint connections..." << std::endl;

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();


    std::cout << "Creating session" << std::endl;

    auto session = MidiSession::Create(L"TAEF TestLoopbackEndpointConnections Session");
    VERIFY_IS_NOT_NULL(session);


    std::cout << "Creating connections" << std::endl;

    auto connectionA = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    VERIFY_IS_NOT_NULL(connectionA);

    auto connectionB = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());
    VERIFY_IS_NOT_NULL(connectionB);

    std::cout << "Creating messages" << std::endl;

    MidiMessage32 messageAToB;
    messageAToB.Timestamp(MidiClock::TimestampConstantSendImmediately());
    messageAToB.Word0(0x23001627);

    MidiMessage64 messageBToA;
    messageBToA.Timestamp(MidiClock::TimestampConstantSendImmediately());
    messageBToA.Word0(0x43001627);
    messageBToA.Word1(0x86753090);

    bool messageAToBReceived{ false};
    bool messageBToAReceived{ false };

    std::cout << "Setting up events" << std::endl;

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

}