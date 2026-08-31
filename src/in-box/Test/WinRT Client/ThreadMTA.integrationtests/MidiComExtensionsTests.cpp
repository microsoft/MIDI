// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

// these are artifacts of the com-extensions-idl project
#include "WindowsMidiServicesAppSdkComExtensions.h"
#include "WindowsMidiServicesAppSdkComExtensions_i.c"

#include "MidiComExtensionsTests.h"

_Use_decl_annotations_
void MidiComExtensionsTests::TestSendReceiveMessagesInternal()
{
    auto sessionThread = GetCurrentThreadId();
    std::cout << "TestSendReceiveMessagesInternal thread: " << sessionThread << std::endl;

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    std::cout << "Creating session" << std::endl;

    auto session = MidiSession::Create(L"COM TestSendReceiveMessagesInternal");
    VERIFY_IS_NOT_NULL(session);
    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    std::cout << "Creating connections" << std::endl;

    auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    //auto connSend = session.CreateEndpointConnection(L"\\\\?\\swd#midisrv#midiu_ksa_11445416705185160265#{e7cce071-3c03-423f-88d3-f1045d02552b}");
    //auto connReceive = session.CreateEndpointConnection(L"\\\\?\\swd#midisrv#midiu_ksa_11445416705185160265#{e7cce071-3c03-423f-88d3-f1045d02552b}");

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    std::cout << "Building messages" << std::endl;

    std::vector<uint32_t> sendBuffer;

    for (int i = 0; i < 0x80; i++)
    {
        // 128 UMPs with note on messages for channel index 0, group index 0
        sendBuffer.push_back(0x20901500 + i);

        // 128 UMPs with note off messages for channel index 0, group index 0
        sendBuffer.push_back(0x20801500 + i);
    }

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());

    uint32_t m_countWordsReceived { 0 };

    std::cout << "Setting callback" << std::endl;

    m_midiInCallback = [&](GUID sessionId, GUID connectionId, UINT64 timestamp, UINT32 wordCount, UINT32* messages)
        {
            UNREFERENCED_PARAMETER(sessionId);
            UNREFERENCED_PARAMETER(connectionId);
            UNREFERENCED_PARAMETER(timestamp);
            UNREFERENCED_PARAMETER(messages);

            m_countWordsReceived += wordCount;

        //    auto callbackThreadId = GetCurrentThreadId();
        //    std::cout << "Message received on lambda COM callback thread: " << callbackThreadId << std::endl;

            if (m_countWordsReceived >= sendBuffer.size())
            {
                allMessagesReceived.SetEvent();
            }
        };

    // shortcut for the equivalent to QueryInterface
    auto receiveConnectionExtension = connReceive.as<IMidiEndpointConnectionRaw>();
    VERIFY_IS_NOT_NULL(receiveConnectionExtension);

    receiveConnectionExtension->SetMessagesReceivedCallback(this);

    //auto receiveConnectionCallbackInterface = static_cast<IMidiEndpointConnectionMessagesReceivedCallback*>(this);
    //VERIFY_IS_NOT_NULL(receiveConnectionCallbackInterface);
    //receiveConnectionExtension->SetMessagesReceivedCallback(receiveConnectionCallbackInterface);


    // shortcut for the equivalent to QueryInterface
    auto sendConnectionExtension = connSend.as<IMidiEndpointConnectionRaw>();
    VERIFY_IS_NOT_NULL(sendConnectionExtension);

    // make sure we're not sending too many words
    auto maxWords = sendConnectionExtension->GetSupportedMaxMidiWordsPerTransmission();
    VERIFY_IS_LESS_THAN_OR_EQUAL(sendBuffer.size(), maxWords);


    auto startTime = MidiClock::Now();

    auto sendResult = sendConnectionExtension->SendMidiMessagesRaw(
        MidiClock::TimestampConstantSendImmediately(),
        static_cast<uint32_t>(sendBuffer.size()),
        sendBuffer.data()       
    );

    VERIFY_SUCCEEDED(sendResult);

    m_countWordsSent += sendBuffer.size();

    // Wait for incoming message
    if (!allMessagesReceived.wait(6000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    auto completionTime = MidiClock::Now();

    auto totalTime = MidiClock::ConvertTimestampTicksToMicroseconds(completionTime - startTime);

    m_totalTime += totalTime;

    // verify that we didn't receive additional words
    VERIFY_ARE_EQUAL(sendBuffer.size(), m_countWordsReceived);

    // unhook our callback and release the COM references
    // you must do this before otherwise shutting down the connection
    // because these hold out-of-band references to the WinRT types
    VERIFY_SUCCEEDED(receiveConnectionExtension->RemoveMessagesReceivedCallback());
    receiveConnectionExtension = nullptr;
    sendConnectionExtension = nullptr;


    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());
    session.Close();

    // if you really want to call uninit_apartment, you must release all your COM and WinRT references first
    // these don't go out of scope here and self-destruct, so we set them to nullptr
    connSend = nullptr;
    connReceive = nullptr;
    session = nullptr;
    
}


void MidiComExtensionsTests::TestSendReceiveMessages()
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    for (int i = 0; i < 50 ; i++)
    {
        std::cout << std::endl << "Testing COM extensions with multi-threaded apartment round " << i+1 << ". " << std::endl << std::endl;
        TestSendReceiveMessagesInternal();
    }

    VERIFY_IS_TRUE(m_countWordsSent > 0);

    std::cout << std::endl << "COM extensions round trip measurements: " << std::endl;
    std::cout << "Total MTA time: " << m_totalTime << " microseconds, for " << m_countWordsSent << " words." << std::endl;
    std::cout << "Average Word time: " << m_totalTime / m_countWordsSent << " microseconds." << std::endl;

}
