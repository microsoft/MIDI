// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "stdafx.h"

#include "winmidi/WindowsMidiServicesAppSdkComExtensions.h"
#include "winmidi/WindowsMidiServicesAppSdkComExtensions_i.c"

#include "MidiComExtensionsTests.h"

void MidiComExtensionsTests::TestSendReceiveMessages()
{
    auto initializer = InitWinRTAndSDK_MTA();

    LOG_OUTPUT(L"TestSendReceiveMessages **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::Create(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    //auto connSend = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackAEndpointDeviceId());
    //auto connReceive = session.CreateEndpointConnection(MidiDiagnostics::DiagnosticsLoopbackBEndpointDeviceId());

    auto connSend = session.CreateEndpointConnection(L"\\\\?\\swd#midisrv#midiu_ksa_11445416705185160265#{e7cce071-3c03-423f-88d3-f1045d02552b}");
    auto connReceive = session.CreateEndpointConnection(L"\\\\?\\swd#midisrv#midiu_ksa_11445416705185160265#{e7cce071-3c03-423f-88d3-f1045d02552b}");


    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

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


    m_midiInCallback = [&](UINT64 timestamp, UINT32 wordCount, UINT32* messages)
        {
            UNREFERENCED_PARAMETER(timestamp);
            UNREFERENCED_PARAMETER(messages);

            m_countWordsReceived += wordCount;

            if (m_countWordsReceived >= sendBuffer.size())
            {
                allMessagesReceived.SetEvent();
            }
        };


    // shortcut for the equivalent to QueryInterface
    auto receiveConnectionExtension = connReceive.as<IMidiEndpointConnectionRaw>();
    VERIFY_IS_NOT_NULL(receiveConnectionExtension);

    receiveConnectionExtension->SetMessagesReceivedCallback(this);


    // shortcut for the equivalent to QueryInterface
    auto sendConnectionExtension = connSend.as<IMidiEndpointConnectionRaw>();
    VERIFY_IS_NOT_NULL(sendConnectionExtension);

    // make sure we're not sending too many words
    auto maxWords = sendConnectionExtension->GetSupportedMaxMidiWordsPerTransmission();
    VERIFY_IS_LESS_THAN_OR_EQUAL(sendBuffer.size(), maxWords);

    auto sendResult = sendConnectionExtension->SendMidiMessagesRaw(
        MidiClock::TimestampConstantSendImmediately(),
        static_cast<uint32_t>(sendBuffer.size()),
        sendBuffer.data()       
    );

    VERIFY_SUCCEEDED(sendResult);


    // Wait for incoming message
    if (!allMessagesReceived.wait(6000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    // verify that we didn't receive additional words
    VERIFY_ARE_EQUAL(sendBuffer.size(), m_countWordsReceived);

    // unhook our callback and release the COM referneces
    // you must do this before otherwise shutting down the connection
    // because these hold out-of-band references to the WinRT types
    receiveConnectionExtension->RemoveMessagesReceivedCallback();
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

    ShutdownSDKAndWinRT(initializer);
}
