// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"


using namespace winrt::Windows::Devices::Midi2;


struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};


void MidiEndpointConnectionBufferTests::TestSendBuffer()
{
    LOG_OUTPUT(L"TestSendBuffer **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());

    VERIFY_IS_NOT_NULL(connSend);


    VERIFY_IS_TRUE(connSend.Open());

    const size_t bufferSizeInBytes = sizeof(uint32_t) * 100;

    winrt::Windows::Foundation::MemoryBuffer sendBuffer(bufferSizeInBytes);
    auto ref = sendBuffer.CreateReference();

    auto interop = ref.as<IMemoryBufferByteAccess>();

    uint8_t* value{};
    uint32_t valueSize{};

    // get a pointer to the buffer
    winrt::check_hresult(interop->GetBuffer(&value, &valueSize));

    // you need to do your normal bounds checking here.
    uint32_t* sendBufferWordPointer = reinterpret_cast<uint32_t*>(value);

    // you can also memcpy or whatever you need to do to get the data in here.
    uint8_t numWords = 2;
    uint32_t byteOffset = 0;
    uint8_t numBytes = numWords * sizeof(uint32_t);

    *sendBufferWordPointer = 0x41234567;
    *(sendBufferWordPointer + 1) = 0xDEADBEEF;

    auto result = connSend.SendMessageBuffer(0, sendBuffer, byteOffset, numBytes);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(result));
    //VERIFY_IS_TRUE((result & MidiSendMessageResult::DataIndexOutOfRange) == MidiSendMessageResult::DataIndexOutOfRange);

    session.DisconnectEndpointConnection(connSend.ConnectionId());

    session.Close();
}

void MidiEndpointConnectionBufferTests::TestSendAndReceiveBuffer()
{
    LOG_OUTPUT(L"TestSendAndReceiveBuffer **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    bool messageReceivedFlag = false;

    uint32_t sentByteOffset = 8 * sizeof(uint32_t);
    uint32_t receiveByteOffset = 5 * sizeof(uint32_t);

    uint8_t sentWordCount = 2;
    uint32_t word0 = 0x41234567;
    uint32_t word1 = 0xDEADBEEF;

    uint8_t sentByteCount = sentWordCount * sizeof(uint32_t);

    const size_t sendBufferSizeInBytes = sizeof(uint32_t) * 20;
    const size_t receiveBufferSizeInBytes = sizeof(uint32_t) * 100;
    winrt::Windows::Foundation::MemoryBuffer receiveBuffer(receiveBufferSizeInBytes);
    winrt::Windows::Foundation::MemoryBuffer sendBuffer(sendBufferSizeInBytes);


    // set up the receive buffer --------------------------------------------
    auto receiveRef = receiveBuffer.CreateReference();
    auto receiveInterop = receiveRef.as<IMemoryBufferByteAccess>();

    uint8_t* receiveBufferBytePointer{};
    uint32_t receiveBufferSize{};

    // get a pointer to the buffer
    winrt::check_hresult(receiveInterop->GetBuffer(&receiveBufferBytePointer, &receiveBufferSize));

    //uint32_t* receiveBufferWordPointer = reinterpret_cast<uint32_t*>(receiveBufferBytePointer + receiveByteOffset);


    // set up the send buffer --------------------------------------------

    auto ref = sendBuffer.CreateReference();

    auto interop = ref.as<IMemoryBufferByteAccess>();

    uint8_t* sendBufferBytePointer{};
    uint32_t sendBufferSize{};

    // get a pointer to the buffer
    winrt::check_hresult(interop->GetBuffer(&sendBufferBytePointer, &sendBufferSize));

    // you need to do your normal bounds checking here.
    uint32_t* sendBufferWordPointer = reinterpret_cast<uint32_t*>(sendBufferBytePointer + sentByteOffset);

    // you can also memcpy or whatever you need to do to get the data in here.
    uint8_t numWords = 2;
    uint8_t numBytes = numWords * sizeof(uint32_t);

    *sendBufferWordPointer = word0;
    *(sendBufferWordPointer + 1) = word1;



    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            VERIFY_IS_NOT_NULL(sender);
            VERIFY_IS_NOT_NULL(args);

            auto receivedByteCount = args.FillBuffer(receiveBuffer, receiveByteOffset);

            VERIFY_ARE_EQUAL(sentByteCount, receivedByteCount);

            // Check that the bytes are written

            for (int i = 0; i < receivedByteCount; i++)
            {
                VERIFY_ARE_EQUAL(*(sendBufferBytePointer + sentByteOffset + i), *(receiveBufferBytePointer + receiveByteOffset + i));
            }

            messageReceivedFlag = true;
            allMessagesReceived.SetEvent();
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());


    auto result = connSend.SendMessageBuffer(0, sendBuffer, sentByteOffset, numBytes);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(result));

    // Wait for incoming message
    if (!allMessagesReceived.wait(3000))
    {
        std::cout << "Failure waiting for messages, timed out." << std::endl;
    }

    VERIFY_IS_TRUE(messageReceivedFlag);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}

void MidiEndpointConnectionBufferTests::TestSendBufferBoundsError()
{
    LOG_OUTPUT(L"TestSendBufferBoundsError **********************************************************************");

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());

    VERIFY_IS_NOT_NULL(connSend);


    VERIFY_IS_TRUE(connSend.Open());

    const size_t bufferSizeInBytes = sizeof(uint32_t) * 100;

    winrt::Windows::Foundation::MemoryBuffer sendBuffer(bufferSizeInBytes);
    auto ref = sendBuffer.CreateReference();

    auto interop = ref.as<IMemoryBufferByteAccess>();

    uint8_t* value{};
    uint32_t valueSize{};

    // get a pointer to the buffer
    winrt::check_hresult(interop->GetBuffer(&value, &valueSize));


    // In this case, we're putting a type 4 UMP (just the first word of it) in the last word of the buffer
    // This should fail a bounds check because a type 4 is two words long
    uint32_t byteOffset = bufferSizeInBytes - sizeof(uint32_t);
    uint32_t* sendBufferWordPointer = reinterpret_cast<uint32_t*>(value + byteOffset);

    // you can also memcpy or whatever you need to do to get the data in here.
    uint8_t numWords = 2;
    uint8_t numBytes = numWords * sizeof(uint32_t);

    *sendBufferWordPointer = 0x41234567;
    //*(sendBufferWordPointer + 1) = 0xDEADBEEF;

    auto result = connSend.SendMessageBuffer(0, sendBuffer, byteOffset, numBytes);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageFailed(result));
    VERIFY_IS_TRUE((result & MidiSendMessageResult::DataIndexOutOfRange) == MidiSendMessageResult::DataIndexOutOfRange);

    session.DisconnectEndpointConnection(connSend.ConnectionId());

    session.Close();





}






void MidiEndpointConnectionBufferTests::TestSendAndReceiveMultipleMessagesBuffer()
{
    LOG_OUTPUT(L"TestSendAndReceiveMultipleMessagesBuffer **********************************************************************");

    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    auto session = MidiSession::CreateSession(L"Test Session Name");

    VERIFY_IS_TRUE(session.IsOpen());
    VERIFY_ARE_EQUAL(session.Connections().Size(), (uint32_t)0);

    auto connSend = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    auto connReceive = session.CreateEndpointConnection(MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());

    VERIFY_IS_NOT_NULL(connSend);
    VERIFY_IS_NOT_NULL(connReceive);

    bool messageReceivedFlag = false;

    uint32_t sentByteOffset = 8 * sizeof(uint32_t);

    uint32_t totalSendMessageCount = 50;



    // allocate over-size buffers
    const uint32_t sendBufferSizeInBytes = (uint32_t)(sizeof(uint32_t) * 4 * (totalSendMessageCount + 10) + sentByteOffset);

    winrt::Windows::Foundation::MemoryBuffer sendBuffer(sendBufferSizeInBytes);

    // set up the send buffer --------------------------------------------

    auto ref = sendBuffer.CreateReference();

    auto interop = ref.as<IMemoryBufferByteAccess>();

    uint8_t* sendBufferBytePointer{};
    uint32_t sendBufferSize{};

    // get a pointer to the buffer
    VERIFY_SUCCEEDED(interop->GetBuffer(&sendBufferBytePointer, &sendBufferSize));

    // you need to do your normal bounds checking here.
    uint32_t* sendBufferWordPointer = reinterpret_cast<uint32_t*>(sendBufferBytePointer + sentByteOffset);
    uint32_t numBytesSent = 0;

    for (uint32_t i = 0; i < totalSendMessageCount; i++)
    {
        uint8_t wordCount = 0;

        if (i % 2 == 0)
        {
            // message 1
            *sendBufferWordPointer =        (uint32_t)0x41234567;
            *(sendBufferWordPointer + 1) =  (uint32_t)0xDEADBEEF;

        //    std::wcout << L"Sending MT: " << std::wstring(MidiMessageUtility::GetMessageFriendlyNameFromFirstWord(*sendBufferWordPointer).c_str()) << std::endl;

            // increment by number of words above.
            wordCount = 2;
            sendBufferWordPointer += wordCount;

            numBytesSent += sizeof(uint32_t) * wordCount;
        }
        else if (i % 3 == 0)
        {
            // message 2
            *sendBufferWordPointer =        (uint32_t)0x28675309;

            //    std::wcout << L"Sending MT: " << std::wstring(MidiMessageUtility::GetMessageFriendlyNameFromFirstWord(*sendBufferWordPointer).c_str()) << std::endl;

            // increment by number of words above.
            wordCount = 1;
            sendBufferWordPointer += wordCount;
            numBytesSent += sizeof(uint32_t) * wordCount;
        }
        else
        {
            // message 3 is type C: 96
            *sendBufferWordPointer =        (uint32_t)0xC0FFEE00;
            *(sendBufferWordPointer + 1) =  (uint32_t)0xF0F0F0F0;
            *(sendBufferWordPointer + 2) =  (uint32_t)0xBEEFBEEF;

            //    std::wcout << L"Sending MT: " << std::wstring(MidiMessageUtility::GetMessageFriendlyNameFromFirstWord(*sendBufferWordPointer).c_str()) << std::endl;

            // increment by number of words above.
            wordCount = 3;
            sendBufferWordPointer += wordCount;
            numBytesSent += sizeof(uint32_t) * wordCount;
        }

    }

    std::cout << "Total messages to send: " << totalSendMessageCount << std::endl;
    std::cout << "Total bytes to send:    " << numBytesSent << std::endl;


    uint32_t countMessagesReceived = 0;
    uint32_t countBytesReceived = 0;

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& /*sender*/, MidiMessageReceivedEventArgs const& args)
        {
            //VERIFY_IS_NOT_NULL(sender);
            //VERIFY_IS_NOT_NULL(args);

            //auto receivedByteCount = args.FillBuffer(receiveBuffer, receiveByteOffset);

         //   std::cout << "Received MT: " << (uint32_t)args.MessageType() << std::endl;

            // increment message count
            countMessagesReceived++;

            countBytesReceived += (uint8_t)MidiMessageUtility::GetPacketTypeFromMessageFirstWord(args.PeekFirstWord()) * sizeof(uint32_t);

            if (countMessagesReceived == totalSendMessageCount)
            {
                messageReceivedFlag = true;
                allMessagesReceived.SetEvent();
            }
        };

    auto eventRevokeToken = connReceive.MessageReceived(MessageReceivedHandler);

    VERIFY_IS_TRUE(connSend.Open());
    VERIFY_IS_TRUE(connReceive.Open());


    auto result = connSend.SendMultipleMessagesBuffer(MidiClock::TimestampConstantSendImmediately(), sendBuffer, sentByteOffset, numBytesSent);

    VERIFY_IS_TRUE(MidiEndpointConnection::SendMessageSucceeded(result));

    // Wait for incoming message
    if (!allMessagesReceived.wait(5000))
    {
        std::cout << "Failure waiting for messages. Timed out." << std::endl;
    }

    VERIFY_IS_TRUE(messageReceivedFlag);

    std::cout << std::endl << "Count messages sent:     [" << totalSendMessageCount << "]" << std::endl;
    std::cout << std::endl << "Count messages received: [" << countMessagesReceived << "]" << std::endl;
    std::cout << std::endl << "Count bytes received:    [" << countBytesReceived << "]" << std::endl;

    VERIFY_IS_TRUE(countMessagesReceived == totalSendMessageCount);

    // unwire event
    connReceive.MessageReceived(eventRevokeToken);

    // cleanup endpoint. Technically not required as session will do it
    session.DisconnectEndpointConnection(connSend.ConnectionId());
    session.DisconnectEndpointConnection(connReceive.ConnectionId());

    session.Close();
}
