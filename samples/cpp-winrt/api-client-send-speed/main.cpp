// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

// Windows MIDI Services sample code

#include "pch.h"
#include <iostream>
#include <iomanip>

using namespace winrt::Windows::Devices::Midi2;        // API

// standard WinRT enumeration support. This is how you find attached devices.
using namespace winrt::Windows::Devices::Enumeration;

// where you find types like IAsyncOperation, IInspectable, etc.
namespace foundation = winrt::Windows::Foundation;
namespace collections = winrt::Windows::Foundation::Collections;

void DisplaySingleResult(std::wstring label, uint64_t totalTime, uint64_t errorCount, uint32_t totalMessageCount)
{
    std::wcout 
        << std::setw(35) << std::left
        << label
        << L"Total: "
        << std::fixed << std::setprecision(2) << std::right
        << MidiClock::ConvertTimestampToMilliseconds(totalTime)
        << "ms, average: "
        << std::fixed << std::setprecision(2) << std::right
        << MidiClock::ConvertTimestampToMicroseconds((uint64_t)((double)totalTime / totalMessageCount))
        << " microseconds per message, "
        << errorCount 
        << " errors"
        << std::endl;

}

#define MESSAGE_COUNT_PER_ITERATION     100
#define ITERATION_COUNT                 2000

#define TOTAL_WORD_COUNT_PER_ITERATION  (MESSAGE_COUNT_PER_ITERATION * 3)


int main()
{
    winrt::init_apartment();

    auto endpointId = MidiEndpointDeviceInformation::DiagnosticsLoopbackAEndpointId();

    uint64_t TotalSendTicksIndividualMessagePackets{ 0 };
    uint64_t TotalSendTicksIndividualMessageWords{ 0 };
    uint64_t TotalSendTicksIndividualMessageStructs{ 0 };

    uint64_t TotalSendTicksVectorMessagePackets{ 0 };
    uint64_t TotalSendTicksVectorMessageWords{ 0 };
    uint64_t TotalSendTicksVectorMessageStructs{ 0 };

    uint64_t TotalSendTicksArrayMessageWords{ 0 };
    uint64_t TotalSendTicksArrayMessageStructs{ 0 };

    uint64_t TotalSendTicksMultipleMessageBuffer{ 0 };

    uint64_t TotalSendErrorsIndividualMessagePackets{ 0 };
    uint64_t TotalSendErrorsIndividualMessageWords{ 0 };
    uint64_t TotalSendErrorsIndividualMessageStructs{ 0 };

    uint64_t TotalSendErrorsVectorMessagePackets{ 0 };
    uint64_t TotalSendErrorsVectorMessageWords{ 0 };
    uint64_t TotalSendErrorsVectorMessageStructs{ 0 };
    
    uint64_t TotalSendErrorsMultipleMessageBuffer{ 0 };

    uint64_t TotalSendErrorsArrayMessageWords{ 0 };
    uint64_t TotalSendErrorsArrayMessageStructs{ 0 };


    // create the MIDI session, giving us access to Windows MIDI Services. An app may open 
    // more than one session. If so, the session name should be meaningful to the user, like
    // the name of a browser tab, or a project.

    auto session = MidiSession::CreateSession(L"Speed Test");

    auto sendEndpoint = session.CreateEndpointConnection(endpointId);

    // once you have wired up all your event handlers, added any filters/listeners, etc.
    // You can open the connection. Doing this will query the cache for the in-protocol 
    // endpoint information and function blocks. If not there, it will send out the requests
    // which will come back asynchronously with responses.
    sendEndpoint.Open();

    std::cout << "Connected to open send endpoint: " << winrt::to_string(endpointId) << std::endl;


    auto ump64 = MidiMessageBuilder::BuildMidi2ChannelVoiceMessage(
        MidiClock::TimestampConstantSendImmediately(),
        MidiGroup(5),      // group 5
        Midi2ChannelVoiceMessageStatus::NoteOn,    
        MidiChannel(3),      // channel 3
        120,    // note 120 - hex 0x78
        100);   // velocity 100 hex 0x64

    auto ump32 = MidiMessageBuilder::BuildMidi1ChannelVoiceMessage(
        MidiClock::TimestampConstantSendImmediately(),
        MidiGroup(5),      // group 5
        Midi1ChannelVoiceMessageStatus::NoteOn,
        MidiChannel(2),      // channel 3
        110,    // note 110
        200);   // velocity 200


    // Build our messages to send out. We do this all at once, and it is not part of the benchmarking
    // this way, we're *only* testing the send speed

    auto messageList = winrt::single_threaded_vector<IMidiUniversalPacket>() ;
    auto wordList = winrt::single_threaded_vector<uint32_t>();
    auto structList = winrt::single_threaded_vector<MidiMessageStruct>();
    auto wordListList = std::vector<collections::IVectorView<uint32_t>>();
    uint32_t wordArray[TOTAL_WORD_COUNT_PER_ITERATION];    // that *3 needs to change if we mix up more than just the ump32 and ump64 per iteration
    MidiMessageStruct structArray[MESSAGE_COUNT_PER_ITERATION];
  




    // if we change the types of messages we send, we need to change this as well
    foundation::MemoryBuffer buffer(TOTAL_WORD_COUNT_PER_ITERATION * sizeof(uint32_t));

  
    uint32_t memoryBufferOffset = 0;
    uint32_t bytesWritten = 0;

    for (uint32_t i = 0; i < MESSAGE_COUNT_PER_ITERATION; i += 2)
    {
        // message list
        messageList.Append(ump64);
        messageList.Append(ump32);

        // word list
        ump64.AppendAllMessageWordsToList(wordList);
        ump32.AppendAllMessageWordsToList(wordList);

        // buffer
        ump64.FillBuffer(memoryBufferOffset, buffer);
        ump32.FillBuffer(memoryBufferOffset, buffer);

        // structs

        MidiMessageStruct str64;
        str64.Word0 = ump64.Word0();
        str64.Word1 = ump64.Word1();
        structList.Append(str64);

        MidiMessageStruct str32;
        str32.Word0 = ump32.Word0();
        structList.Append(str32);

        // for sending words one message at a time
        wordListList.push_back(ump64.GetAllWords().GetView());
        wordListList.push_back(ump32.GetAllWords().GetView());

        bytesWritten += sizeof(ump64) + sizeof(ump32);

        memoryBufferOffset += bytesWritten;
    }




    // Actually send the messages

    for (uint32_t i = 0; i < ITERATION_COUNT; i++)
    {
        uint64_t startTick{};
        uint64_t stopTick{};

        MidiSendMessageResults result;

        // individual message tests ---------------------------------------------------------------------

        // send individual message words
        startTick = MidiClock::Now();

        for (auto const& message : wordListList)
        {
            if (message.Size() == 4)
                result = sendEndpoint.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), message.GetAt(0), message.GetAt(1), message.GetAt(2), message.GetAt(3));
            else if (message.Size() == 3)
                result = sendEndpoint.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), message.GetAt(0), message.GetAt(1), message.GetAt(2));
            else if (message.Size() == 2)
                result = sendEndpoint.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), message.GetAt(0), message.GetAt(1));
            else if (message.Size() == 1)
                result = sendEndpoint.SendSingleMessageWords(MidiClock::TimestampConstantSendImmediately(), message.GetAt(0));

            if (MidiEndpointConnection::SendMessageFailed(result))
            {
                TotalSendErrorsIndividualMessageWords++;
            }

        }
        stopTick = MidiClock::Now();
        TotalSendTicksIndividualMessageWords += (stopTick - startTick);



        // send individual structs

        startTick = MidiClock::Now();
        for (auto const& message : structList)
        {
            result = sendEndpoint.SendSingleMessageStruct(MidiClock::TimestampConstantSendImmediately(), (uint8_t)MidiMessageUtility::GetPacketTypeFromMessageFirstWord(message.Word0), message);
            if (MidiEndpointConnection::SendMessageFailed(result))
            {
                TotalSendErrorsIndividualMessageStructs++;
            }
        }
        stopTick = MidiClock::Now();
        TotalSendTicksIndividualMessageStructs += (stopTick - startTick);



        // send individual message packets

        startTick = MidiClock::Now();
        for (auto const& message : messageList)
        {
            result = sendEndpoint.SendSingleMessagePacket(message);
            if (MidiEndpointConnection::SendMessageFailed(result))
            {
                TotalSendErrorsIndividualMessagePackets++;
            }
        }
        stopTick = MidiClock::Now();
        TotalSendTicksIndividualMessagePackets += (stopTick - startTick);


        // send individual buffers





        // multi-message tests --------------------------------------------------------------------------

        // send vector of words

        startTick = MidiClock::Now();
        result = sendEndpoint.SendMultipleMessagesWordList(MidiClock::TimestampConstantSendImmediately(), wordList);
        if (MidiEndpointConnection::SendMessageFailed(result))
        {
            TotalSendErrorsVectorMessageWords++;
        }
        stopTick = MidiClock::Now();
        TotalSendTicksVectorMessageWords += (stopTick - startTick);

        // send array of words
        
        startTick = MidiClock::Now();
        result = sendEndpoint.SendMultipleMessagesWordArray(MidiClock::TimestampConstantSendImmediately(), 0, TOTAL_WORD_COUNT_PER_ITERATION, wordArray);
        if (MidiEndpointConnection::SendMessageFailed(result))
        {
            TotalSendErrorsArrayMessageWords++;
        }
        stopTick = MidiClock::Now();
        TotalSendTicksArrayMessageWords += (stopTick - startTick);

         
        // send vector of message packets

        startTick = MidiClock::Now();
        result = sendEndpoint.SendMultipleMessagesPacketList(messageList.GetView());
        if (MidiEndpointConnection::SendMessageFailed(result))
        {
            TotalSendErrorsVectorMessagePackets++;
        }
        stopTick = MidiClock::Now();
        TotalSendTicksVectorMessagePackets += (stopTick - startTick);


        // send vector of message structs

        startTick = MidiClock::Now();
        result = sendEndpoint.SendMultipleMessagesStructList(MidiClock::TimestampConstantSendImmediately(), structList.GetView());
        if (MidiEndpointConnection::SendMessageFailed(result))
        {
            TotalSendErrorsVectorMessageStructs++;
        }
        stopTick = MidiClock::Now();
        TotalSendTicksVectorMessageStructs += (stopTick - startTick);

        // send array of message structs

        startTick = MidiClock::Now();
        result = sendEndpoint.SendMultipleMessagesStructArray(MidiClock::TimestampConstantSendImmediately(), 0, MESSAGE_COUNT_PER_ITERATION, structArray);
        if (MidiEndpointConnection::SendMessageFailed(result))
        {
            TotalSendErrorsArrayMessageStructs++;
        }
        stopTick = MidiClock::Now();
        TotalSendTicksArrayMessageStructs += (stopTick - startTick);


        // send multiple through buffer

        startTick = MidiClock::Now();
        result = sendEndpoint.SendMultipleMessagesBuffer(MidiClock::TimestampConstantSendImmediately(), 0, bytesWritten, buffer);
        if (MidiEndpointConnection::SendMessageFailed(result))
        {
            TotalSendErrorsMultipleMessageBuffer++;
        }
        stopTick = MidiClock::Now();
        TotalSendTicksMultipleMessageBuffer += (stopTick - startTick);
    }


    // let's see how we did

    uint32_t totalMessageCount = ITERATION_COUNT * MESSAGE_COUNT_PER_ITERATION;

    std::wcout << std::endl;
    std::wcout << L"Iteration Count:        " << ITERATION_COUNT << std::endl;
    std::wcout << L"Messages per Iteration: " << MESSAGE_COUNT_PER_ITERATION << std::endl;
    std::wcout << L"Total Message Count:    " << totalMessageCount << std::endl;

    std::wcout << std::endl << L"Multiple messages per call" << std::endl;
    std::wcout << L"----------------------------------------------------------------------------------------" << std::endl;
    DisplaySingleResult(L"SendMultipleMessagesPacketList", TotalSendTicksVectorMessagePackets, TotalSendErrorsVectorMessagePackets, totalMessageCount);

    DisplaySingleResult(L"SendMultipleMessagesStructList", TotalSendTicksVectorMessageStructs, TotalSendErrorsVectorMessageStructs, totalMessageCount);
    DisplaySingleResult(L"SendMultipleMessagesStructArray", TotalSendTicksArrayMessageStructs, TotalSendErrorsArrayMessageStructs, totalMessageCount);

    DisplaySingleResult(L"SendMultipleMessagesWordList", TotalSendTicksVectorMessageWords, TotalSendErrorsVectorMessageWords, totalMessageCount);
    DisplaySingleResult(L"SendMultipleMessagesWordArray", TotalSendTicksArrayMessageWords, TotalSendErrorsArrayMessageWords, totalMessageCount);

    DisplaySingleResult(L"SendMultipleMessagesBuffer", TotalSendTicksMultipleMessageBuffer, TotalSendErrorsMultipleMessageBuffer, totalMessageCount);

    std::wcout << std::endl << L"Single message per call" << std::endl;
    std::wcout << L"----------------------------------------------------------------------------------------" << std::endl;
    DisplaySingleResult(L"SendMessagePacket", TotalSendTicksIndividualMessagePackets, TotalSendErrorsIndividualMessagePackets, totalMessageCount);
    DisplaySingleResult(L"SendMessageStruct", TotalSendTicksIndividualMessageStructs, TotalSendErrorsIndividualMessageStructs, totalMessageCount);
    DisplaySingleResult(L"SendMessageWords", TotalSendTicksIndividualMessageWords, TotalSendErrorsIndividualMessageWords, totalMessageCount);

    // shut down

    std::cout << std::endl << "Disconnecting UMP Endpoint Connection..." << std::endl;

    session.DisconnectEndpointConnection(sendEndpoint.ConnectionId());
    // close the session, detaching all Windows MIDI Services resources and closing all connections
    // You can also disconnect individual Endpoint Connections when you are done with them, as we did above
    session.Close();

}
