// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "stdafx.h"


void MidiEndpointListenerTests::TestMessageTypeListener()
{
    LOG_OUTPUT(L"TestMessageTypeListener **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t expectedMatchingMessageCount{ 0 }; // this is set down in the message sending section
    uint32_t receivedMatchingMessageCount{ 0 };



    auto session = MidiSession::TryCreate(L"Listener Session Test");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.TryCreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.TryCreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    // add our listener

    MidiMessageTypeEndpointListener endpointListener;
    endpointListener.IncludedMessageTypes().Append(MidiMessageType::Midi1ChannelVoice32);
    endpointListener.IncludedMessageTypes().Append(MidiMessageType::Midi2ChannelVoice64);

    connReceive.AddMessageProcessingPlugin(endpointListener);

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(args);
            VERIFY_IS_NOT_NULL(sender);

            auto receivedUmp = args.GetMessagePacket();

            auto word0 = args.PeekFirstWord();

            std::cout << "Received message in test" << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (args.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (int)(args.MessageType()) << std::endl;
            std::cout << " - Word0:             0x" << std::hex << (word0) << std::endl;

            uint32_t index;
            if (endpointListener.IncludedMessageTypes().IndexOf(receivedUmp.MessageType(), index))
            {
                std::cout << " - Message type MATCHES filter" << std::endl;

                receivedMatchingMessageCount++;

                if (receivedMatchingMessageCount == expectedMatchingMessageCount)
                {
                    allMessagesReceived.SetEvent();
                }
            }
            else
            {
                std::cout << " - Message type DOES NOT MATCH filter" << std::endl;
            }

        };

    // we wire up to the listener here, not the connection
    auto eventRevokeToken = endpointListener.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    // send messages

    std::cout << "Sending messages" << std::endl;

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x1DEDBEEF))); // no match
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x41234567, 0x12345678)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x23263827)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0xF1234567, 0x00000000, 0x11111111, 0x12345678)));  // no match
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x48675309, 0x12345678)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x28675309)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x0BEEFDED)));  // no match

    expectedMatchingMessageCount = 4;


    // Wait for incoming message
    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    VERIFY_ARE_EQUAL(expectedMatchingMessageCount, receivedMatchingMessageCount);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}


void MidiEndpointListenerTests::TestGroupListener()
{
    LOG_OUTPUT(L"TestGroupListener **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t expectedMatchingMessageCount{ 0 }; // this is set down in the message sending section
    uint32_t receivedMatchingMessageCount{ 0 };



    auto session = MidiSession::TryCreate(L"Listener Session Test");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.TryCreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.TryCreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    // add our listener

    MidiGroupEndpointListener endpointListener;
    endpointListener.IncludedGroups().Append(MidiGroup{ 0x3 });
    endpointListener.IncludedGroups().Append(MidiGroup{ 0xB });

    connReceive.AddMessageProcessingPlugin(endpointListener);

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto receivedUmp = args.GetMessagePacket();

            auto word0 = args.PeekFirstWord();

            std::cout << "Received message in test" << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (args.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (int)(args.MessageType()) << std::endl;
            std::cout << " - Word0:             0x" << std::hex << (word0) << std::endl;

            if (MidiMessageHelper::MessageTypeHasGroupField(args.MessageType()))
            {
                auto messageGroup = MidiMessageHelper::GetGroupFromMessageFirstWord(word0);

                for (auto const& group : endpointListener.IncludedGroups())
                {
                    if (group.Index() == messageGroup.Index())
                    {
                        std::cout << " - Group MATCHES filter" << std::endl;

                        receivedMatchingMessageCount++;

                        if (receivedMatchingMessageCount == expectedMatchingMessageCount)
                        {
                            allMessagesReceived.SetEvent();
                        }

                        return;
                    }
                }

                std::cout << " - Group DOES NOT MATCH filter" << std::endl;
            }
            else
            {
                std::cout << " - Message Type does not have a group field" << std::endl;
            }


        };

    // we wire up to the listener here, not the connection
    auto eventRevokeToken = endpointListener.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    // send messages

    std::cout << "Sending messages" << std::endl;

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x1DEDBEEF))); // no match
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x4B234567, 0x12345678)));  // match, CV message
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x23263827)));              // match, CV message
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0xF1234567, 0x00000000, 0x11111111, 0x12345678)));  // no match
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x43675309, 0x12345678)));  // match, CV message
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x23675309)));              // match, CV message
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x03EEFDED)));  // no match because of message type

    expectedMatchingMessageCount = 4;


    // Wait for incoming message
    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    VERIFY_ARE_EQUAL(expectedMatchingMessageCount, receivedMatchingMessageCount);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}

void MidiEndpointListenerTests::TestGroupAndChannelListener()
{
    LOG_OUTPUT(L"TestGroupAndChannelListener **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t expectedMatchingMessageCount{ 0 }; // this is set down in the message sending section
    uint32_t receivedMatchingMessageCount{ 0 };



    auto session = MidiSession::TryCreate(L"Listener Session Test");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.TryCreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.TryCreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    // add our listener

    MidiChannelEndpointListener endpointListener;
    endpointListener.IncludedGroup(MidiGroup(5));
    endpointListener.IncludedChannels().Append(MidiChannel{ 0x3 });
    endpointListener.IncludedChannels().Append(MidiChannel{ 0xB });

    connReceive.AddMessageProcessingPlugin(endpointListener);

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto receivedUmp = args.GetMessagePacket();

            auto word0 = args.PeekFirstWord();

            std::cout << "Received message in test" << std::endl;
            std::cout << " - Timestamp:         0x" << std::hex << (args.Timestamp()) << std::endl;
            std::cout << " - MessageType:       0x" << std::hex << (int)(args.MessageType()) << std::endl;
            std::cout << " - Word0:             0x" << std::hex << (word0) << std::endl;

            if (MidiMessageHelper::MessageTypeHasChannelField(args.MessageType()))
            {
                auto messageGroup = MidiMessageHelper::GetGroupFromMessageFirstWord(word0);
                auto messageChannel = MidiMessageHelper::GetChannelFromMessageFirstWord(word0);

                if (endpointListener.IncludedGroup().Index() == messageGroup.Index())
                {
                    for (auto const& channel : endpointListener.IncludedChannels())
                    {
                        if (channel.Index() == messageChannel.Index())
                        {
                            std::cout << " - Channel MATCHES filter" << std::endl;

                            receivedMatchingMessageCount++;

                            if (receivedMatchingMessageCount == expectedMatchingMessageCount)
                            {
                                allMessagesReceived.SetEvent();
                            }

                            return;
                        }
                    }
                }
                else
                {
                    std::cout << " - Group DOES NOT MATCH filter" << std::endl;
                }
            }
            else
            {
                std::cout << " - Message Type does not have a channel field" << std::endl;
            }


        };

    // we wire up to the listener here, not the connection
    auto eventRevokeToken = endpointListener.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    // send messages

    std::cout << "Sending messages" << std::endl;

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x15EDBEEF)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x45234567, 0x12345678)));  // match
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x252B3827)));              // match
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0xF5234567, 0x00000000, 0x11111111, 0x12345678))); // not a match, type F
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x44635309, 0x12345678)));  // not a match, wrong group
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x24635309)));              // not a match, wrong group
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), 0x04EBFDED)));              // not a match, due to message type

    expectedMatchingMessageCount = 2;


    // Wait for incoming message
    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    VERIFY_ARE_EQUAL(expectedMatchingMessageCount, receivedMatchingMessageCount);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}

