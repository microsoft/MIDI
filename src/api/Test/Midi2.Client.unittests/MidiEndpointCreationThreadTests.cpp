
#include "stdafx.h"

#include "MidiEndpointCreationThreadTests.h"


#include <wil\resource.h>


#define NUM_MESSAGES_TO_TRANSMIT 10

_Use_decl_annotations_
void MidiEndpointCreationThreadTests::ReceiveThreadWorker(MidiSession session, winrt::hstring endpointId)
{
    wil::unique_event_nothrow allMessagesReceived;
    allMessagesReceived.create();

    uint32_t countMessagesReceived = 0;

    auto connectionThreadId = GetCurrentThreadId();
    std::cout << "Receiver: connection thread: " << connectionThreadId << std::endl;

    // create the connection
    auto connection = session.CreateEndpointConnection(endpointId);

    auto MessageReceivedHandler = [&](IMidiMessageReceivedEventSource const& sender, MidiMessageReceivedEventArgs const& args)
        {
            UNREFERENCED_PARAMETER(sender);
            UNREFERENCED_PARAMETER(args);

            auto callbackThreadId = GetCurrentThreadId();
            std::cout << "Message received on callback thread: " << callbackThreadId << std::endl;

            countMessagesReceived++;

            // this will not be true. The thread receiving messages is a high-priority thread the application does not control.
            //VERIFY_ARE_EQUAL(connectionThreadId, callbackThreadId);

            if (countMessagesReceived >= NUM_MESSAGES_TO_TRANSMIT)
            {
                allMessagesReceived.SetEvent();
            }
            
        };

    auto revoke = connection.MessageReceived(MessageReceivedHandler);
    std::cout << "Receiver: Event handler created." << std::endl;

    connection.Open();
    std::cout << "Receiver: Connection opened." << std::endl;

    m_receiverReady.SetEvent();

    std::cout << "Receiver: Waiting for messages." << std::endl;

    if (!allMessagesReceived.wait(10000))
    {
        std::cout << "Receiver: Failure waiting for receiver to connect." << std::endl;

        VERIFY_FAIL();
    }

    std::cout << "Receiver: " << countMessagesReceived << " messages received." << std::endl;

    m_receiveComplete.SetEvent();
}


_Use_decl_annotations_
void MidiEndpointCreationThreadTests::SendThreadWorker(MidiSession session, winrt::hstring endpointId)
{
    auto threadId = GetCurrentThreadId();

    std::cout << "Sender thread: " << threadId << std::endl;

    // create the connection
    auto connection = session.CreateEndpointConnection(endpointId);
    connection.Open();

    for (uint32_t i = 0; i < NUM_MESSAGES_TO_TRANSMIT; i++)
    {
        connection.SendMessageWords(MidiClock::TimestampConstantSendImmediately(), 0x21234567);
        
    }

    std::cout << "Sender: " << NUM_MESSAGES_TO_TRANSMIT << " messages sent" << std::endl;

}



void MidiEndpointCreationThreadTests::TestCreateNewSessionMultithreaded()
{
    m_receiveComplete.create();
    m_receiverReady.create();

    auto sessionThreadId = GetCurrentThreadId();

    std::cout << "Session thread: " << sessionThreadId << std::endl;

    // create session on this thread

    auto session = MidiSession::CreateSession(L"Multi-threaded Test");

    // create loopback A on thread A
    std::thread workerThreadA(&MidiEndpointCreationThreadTests::ReceiveThreadWorker, this, session, MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId());
    workerThreadA.detach();

    if (!m_receiverReady.wait(10000))
    {
        std::cout << "Session: Failure waiting for receiver to connect." << std::endl;

        VERIFY_FAIL();
    }
    else
    {
        std::cout << "Session: Receiver ready notification received." << std::endl;
    }

    // create loopback B on thread B
    std::thread workerThreadB(&MidiEndpointCreationThreadTests::SendThreadWorker, this, session, MidiEndpointDeviceInformation::DiagnosticsLoopbackBEndpointId());
    workerThreadB.detach();

    if (!m_receiveComplete.wait(20000))
    {
        std::cout << "Session: Failure waiting for all messages to be received." << std::endl;

        VERIFY_FAIL();
    }
    else
    {
        std::cout << "Session: Message receive complete notification received." << std::endl;
    }


}



