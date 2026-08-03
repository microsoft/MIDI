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
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t expectedMatchingMessageCount{ 0 }; // this is set down in the message sending section
    uint32_t receivedMatchingMessageCount{ 0 };



    auto session = MidiSession::Create(L"TestMessageTypeListener");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

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

    // if you really want to call uninit_apartment, you must release all your COM and WinRT references first
    // these don't go out of scope here and self-destruct, so we set them to nullptr
    endpointListener = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}


void MidiEndpointListenerTests::TestGroupListener()
{
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t expectedMatchingMessageCount{ 0 }; // this is set down in the message sending section
    uint32_t receivedMatchingMessageCount{ 0 };

    auto session = MidiSession::Create(L"TestGroupListener");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

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

    // if you really want to call uninit_apartment, you must release all your COM and WinRT references first
    // these don't go out of scope here and self-destruct, so we set them to nullptr
    endpointListener = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}

void MidiEndpointListenerTests::TestGroupAndChannelListener()
{
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t expectedMatchingMessageCount{ 0 }; // this is set down in the message sending section
    uint32_t receivedMatchingMessageCount{ 0 };



    auto session = MidiSession::Create(L"Listener Session Test");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

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

    // if you really want to call uninit_apartment, you must release all your COM and WinRT references first
    // these don't go out of scope here and self-destruct, so we set them to nullptr
    endpointListener = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}


// Message words used by the tests below. All MIDI 1.0 Channel Voice 32 (message type 2)
// on group 5 (the second nibble), with a Note On status (0x9).
#define TEST_MSG_GROUP5_CHANNEL3_A      0x25930001
#define TEST_MSG_GROUP5_CHANNEL3_B      0x25930002
#define TEST_MSG_GROUP5_CHANNELB_A      0x259B0003
#define TEST_MSG_GROUP5_CHANNEL5_A      0x25950004
#define TEST_MSG_GROUP4_CHANNEL3_A      0x24930005

// System Real Time (message type 1) Timing Clock (status 0xF8) on group 5
#define TEST_MSG_GROUP5_CLOCK           0x15F80000


void MidiEndpointListenerTests::TestMultipleChannelListeners()
{
    // Two channel listeners on the same connection, each watching a different
    // channel. Each must receive only the messages for its own channel.

    wil::critical_section receivedLock;
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    std::vector<uint32_t> receivedByListener3;
    std::vector<uint32_t> receivedByListenerB;

    const uint32_t expectedTotalMessageCount = 3;

    auto session = MidiSession::Create(L"TestMultipleChannelListeners");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    // listener for group 5, channel 3
    MidiChannelEndpointListener listenerChannel3;
    listenerChannel3.IncludedGroup(MidiGroup(5));
    listenerChannel3.IncludedChannels().Append(MidiChannel{ 0x3 });

    // listener for group 5, channel 11 (0xB)
    MidiChannelEndpointListener listenerChannelB;
    listenerChannelB.IncludedGroup(MidiGroup(5));
    listenerChannelB.IncludedChannels().Append(MidiChannel{ 0xB });

    connReceive.AddMessageProcessingPlugin(listenerChannel3);
    connReceive.AddMessageProcessingPlugin(listenerChannelB);

    auto signalIfComplete = [&]()
        {
            if (receivedByListener3.size() + receivedByListenerB.size() >= expectedTotalMessageCount)
            {
                allMessagesReceived.SetEvent();
            }
        };

    auto token3 = listenerChannel3.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Channel 3 listener received: 0x" << std::hex << word0 << std::dec << std::endl;

            receivedByListener3.push_back(word0);
            signalIfComplete();
        });

    auto tokenB = listenerChannelB.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Channel B listener received: 0x" << std::hex << word0 << std::dec << std::endl;

            receivedByListenerB.push_back(word0);
            signalIfComplete();
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    std::cout << "Sending messages" << std::endl;

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL3_A)));   // listener 3
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNELB_A)));   // listener B
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL3_B)));   // listener 3
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL5_A)));   // neither: wrong channel
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP4_CHANNEL3_A)));   // neither: wrong group

    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    // give any incorrectly-routed messages a chance to arrive so we can catch them
    Sleep(500);

    {
        auto lock = receivedLock.lock();

        // channel 3 listener should have received exactly its two messages, in order
        VERIFY_ARE_EQUAL(receivedByListener3.size(), (size_t)2);
        VERIFY_ARE_EQUAL(receivedByListener3[0], (uint32_t)TEST_MSG_GROUP5_CHANNEL3_A);
        VERIFY_ARE_EQUAL(receivedByListener3[1], (uint32_t)TEST_MSG_GROUP5_CHANNEL3_B);

        // channel B listener should have received exactly its one message
        VERIFY_ARE_EQUAL(receivedByListenerB.size(), (size_t)1);
        VERIFY_ARE_EQUAL(receivedByListenerB[0], (uint32_t)TEST_MSG_GROUP5_CHANNELB_A);
    }

    listenerChannel3.MessageReceived(token3);
    listenerChannelB.MessageReceived(tokenB);

    connReceive.RemoveMessageProcessingPlugin(listenerChannel3.PluginId());
    connReceive.RemoveMessageProcessingPlugin(listenerChannelB.PluginId());

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();

    listenerChannel3 = nullptr;
    listenerChannelB = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}


void MidiEndpointListenerTests::TestChannelListenerIncludingSystemCommonAndRealTime()
{
    // A channel listener with IncludeSystemCommonAndRealTimeMessages set to true
    // must receive System Real Time messages (MIDI clock) in addition to the
    // channel voice messages for its channel.

    wil::critical_section receivedLock;
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t receivedChannelMessageCount{ 0 };
    uint32_t receivedClockMessageCount{ 0 };

    const uint32_t expectedChannelMessageCount = 1;
    const uint32_t expectedClockMessageCount = 3;

    auto session = MidiSession::Create(L"TestChannelListenerIncludingSystemCommonAndRealTime");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    MidiChannelEndpointListener endpointListener;
    endpointListener.IncludedGroup(MidiGroup(5));
    endpointListener.IncludedChannels().Append(MidiChannel{ 0x3 });
    endpointListener.IncludeSystemCommonAndRealTimeMessages(true);

    VERIFY_IS_TRUE(endpointListener.IncludeSystemCommonAndRealTimeMessages());

    connReceive.AddMessageProcessingPlugin(endpointListener);

    auto eventRevokeToken = endpointListener.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Listener received: 0x" << std::hex << word0 << std::dec << std::endl;

            if (word0 == (uint32_t)TEST_MSG_GROUP5_CLOCK)
            {
                receivedClockMessageCount++;
            }
            else if (word0 == (uint32_t)TEST_MSG_GROUP5_CHANNEL3_A)
            {
                receivedChannelMessageCount++;
            }

            if (receivedClockMessageCount >= expectedClockMessageCount &&
                receivedChannelMessageCount >= expectedChannelMessageCount)
            {
                allMessagesReceived.SetEvent();
            }
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    std::cout << "Sending messages" << std::endl;

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL3_A)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CLOCK)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CLOCK)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CLOCK)));

    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    {
        auto lock = receivedLock.lock();

        // the real-time clock messages must have made it through
        VERIFY_ARE_EQUAL(receivedClockMessageCount, expectedClockMessageCount);

        // and the normal channel message must still be delivered
        VERIFY_ARE_EQUAL(receivedChannelMessageCount, expectedChannelMessageCount);
    }

    endpointListener.MessageReceived(eventRevokeToken);
    connReceive.RemoveMessageProcessingPlugin(endpointListener.PluginId());

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();

    endpointListener = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}


void MidiEndpointListenerTests::TestChannelListenerExcludingSystemCommonAndRealTime()
{
    // The opposite of the previous test. With IncludeSystemCommonAndRealTimeMessages
    // left at its default (false), the listener must NOT receive MIDI clock messages.

    wil::critical_section receivedLock;
    wil::unique_event_nothrow channelMessageReceived;
    channelMessageReceived.create();

    uint32_t receivedChannelMessageCount{ 0 };
    uint32_t receivedClockMessageCount{ 0 };

    auto session = MidiSession::Create(L"TestChannelListenerExcludingSystemCommonAndRealTime");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    MidiChannelEndpointListener endpointListener;
    endpointListener.IncludedGroup(MidiGroup(5));
    endpointListener.IncludedChannels().Append(MidiChannel{ 0x3 });
    endpointListener.IncludeSystemCommonAndRealTimeMessages(false);

    VERIFY_IS_FALSE(endpointListener.IncludeSystemCommonAndRealTimeMessages());

    connReceive.AddMessageProcessingPlugin(endpointListener);

    auto eventRevokeToken = endpointListener.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Listener received: 0x" << std::hex << word0 << std::dec << std::endl;

            if (word0 == (uint32_t)TEST_MSG_GROUP5_CLOCK)
            {
                receivedClockMessageCount++;
            }
            else if (word0 == (uint32_t)TEST_MSG_GROUP5_CHANNEL3_A)
            {
                receivedChannelMessageCount++;
                channelMessageReceived.SetEvent();
            }
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    std::cout << "Sending messages" << std::endl;

    // send the clocks first, then the channel message. Once the channel message
    // arrives we know the clocks have already been processed by the pipeline.
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CLOCK)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CLOCK)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CLOCK)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL3_A)));

    if (!channelMessageReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    // give any incorrectly-routed clock messages a chance to arrive so we can catch them
    Sleep(500);

    {
        auto lock = receivedLock.lock();

        // the channel message must have been received
        VERIFY_ARE_EQUAL(receivedChannelMessageCount, (uint32_t)1);

        // but no real-time clock messages should have been delivered
        VERIFY_ARE_EQUAL(receivedClockMessageCount, (uint32_t)0);
    }

    endpointListener.MessageReceived(eventRevokeToken);
    connReceive.RemoveMessageProcessingPlugin(endpointListener.PluginId());

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();

    endpointListener = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}


void MidiEndpointListenerTests::TestPreventCallingFurtherListeners()
{
    // Three listeners, all matching the same group and channel, added in order.
    // Listener 2 sets PreventCallingFurtherListeners, so listeners 1 and 2 must
    // receive the messages, and listener 3 must receive nothing.

    wil::critical_section receivedLock;
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    std::vector<uint32_t> receivedByListener1;
    std::vector<uint32_t> receivedByListener2;
    std::vector<uint32_t> receivedByListener3;

    const uint32_t expectedMessageCount = 2;

    auto session = MidiSession::Create(L"TestPreventCallingFurtherListeners");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());

    LOG_OUTPUT(L"Connecting to both Loopback A and Loopback B");

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    // listener 1: passes messages on to the next listener
    MidiChannelEndpointListener listener1;
    listener1.IncludedGroup(MidiGroup(5));
    listener1.IncludedChannels().Append(MidiChannel{ 0x3 });
    listener1.PreventCallingFurtherListeners(false);

    // listener 2: receives the message, but stops any further plugin processing
    MidiChannelEndpointListener listener2;
    listener2.IncludedGroup(MidiGroup(5));
    listener2.IncludedChannels().Append(MidiChannel{ 0x3 });
    listener2.PreventCallingFurtherListeners(true);

    // listener 3: would match, but should never be called because of listener 2
    MidiChannelEndpointListener listener3;
    listener3.IncludedGroup(MidiGroup(5));
    listener3.IncludedChannels().Append(MidiChannel{ 0x3 });
    listener3.PreventCallingFurtherListeners(false);

    VERIFY_IS_FALSE(listener1.PreventCallingFurtherListeners());
    VERIFY_IS_TRUE(listener2.PreventCallingFurtherListeners());
    VERIFY_IS_FALSE(listener3.PreventCallingFurtherListeners());

    // order of addition is the order in which the plugins are called
    connReceive.AddMessageProcessingPlugin(listener1);
    connReceive.AddMessageProcessingPlugin(listener2);
    connReceive.AddMessageProcessingPlugin(listener3);

    auto token1 = listener1.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Listener 1 received: 0x" << std::hex << word0 << std::dec << std::endl;

            receivedByListener1.push_back(word0);
        });

    auto token2 = listener2.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Listener 2 received: 0x" << std::hex << word0 << std::dec << std::endl;

            receivedByListener2.push_back(word0);

            if (receivedByListener2.size() >= expectedMessageCount)
            {
                allMessagesReceived.SetEvent();
            }
        });

    auto token3 = listener3.MessageReceived([&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto lock = receivedLock.lock();

            auto word0 = args.PeekFirstWord();
            std::cout << "Listener 3 received (UNEXPECTED): 0x" << std::hex << word0 << std::dec << std::endl;

            receivedByListener3.push_back(word0);
        });

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    std::cout << "Sending messages" << std::endl;

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL3_A)));
    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(connSend.SendSingleMessageWords(MidiClock::Now(), TEST_MSG_GROUP5_CHANNEL3_B)));

    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    // give listener 3 a chance to (incorrectly) receive anything so we can catch it
    Sleep(500);

    {
        auto lock = receivedLock.lock();

        // listener 1 runs before the listener which sets the flag, so it gets everything
        VERIFY_ARE_EQUAL(receivedByListener1.size(), (size_t)expectedMessageCount);
        VERIFY_ARE_EQUAL(receivedByListener1[0], (uint32_t)TEST_MSG_GROUP5_CHANNEL3_A);
        VERIFY_ARE_EQUAL(receivedByListener1[1], (uint32_t)TEST_MSG_GROUP5_CHANNEL3_B);

        // listener 2 sets the flag, but still receives the message itself
        VERIFY_ARE_EQUAL(receivedByListener2.size(), (size_t)expectedMessageCount);
        VERIFY_ARE_EQUAL(receivedByListener2[0], (uint32_t)TEST_MSG_GROUP5_CHANNEL3_A);
        VERIFY_ARE_EQUAL(receivedByListener2[1], (uint32_t)TEST_MSG_GROUP5_CHANNEL3_B);

        // listener 3 must never be called because listener 2 stopped the chain
        VERIFY_ARE_EQUAL(receivedByListener3.size(), (size_t)0);
    }

    listener1.MessageReceived(token1);
    listener2.MessageReceived(token2);
    listener3.MessageReceived(token3);

    connReceive.RemoveMessageProcessingPlugin(listener1.PluginId());
    connReceive.RemoveMessageProcessingPlugin(listener2.PluginId());
    connReceive.RemoveMessageProcessingPlugin(listener3.PluginId());

    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();

    listener1 = nullptr;
    listener2 = nullptr;
    listener3 = nullptr;
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
}

