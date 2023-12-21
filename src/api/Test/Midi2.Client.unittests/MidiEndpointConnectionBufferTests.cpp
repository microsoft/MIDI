// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "stdafx.h"


#include "MidiEndpointConnectionBufferTests.h"


#include <wil\resource.h>

using namespace winrt::Windows::Devices::Midi2;


struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};


void MidiEndpointConnectionBufferTests::TestSendBuffer()
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
    VERIFY_FAIL();
}
