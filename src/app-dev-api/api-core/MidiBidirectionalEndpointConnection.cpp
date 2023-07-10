// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiBidirectionalEndpointConnection.h"
#include "MidiBidirectionalEndpointConnection.g.cpp"

// TEMP
#include <iostream>


namespace winrt::Windows::Devices::Midi2::implementation
{
    
    
    // This implementation will need to change ---------------------------------------------------------------------------

    IFACEMETHODIMP MidiBidirectionalEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    {
        std::cout << __FUNCTION__ << " message(s) received" << std::endl;


        // check if we have any filters. If not, just copy all the data over

        // if we do have filters, check filter strategy


        // intent is to have one UMP at a time, but we will treat this as potentially multiple to allow for that in the future

        // read timestamp

        // read first byte of message to determine how many MIDI words to read

        // evaluate message

        // if passed eval, add it to the queue of incoming messages for this endpoint

        if (_messagesReceivedEvent)
        {
            //   auto args = winrt::make_self<MidiMessagesReceivedEventArgs>();

            // todo, set up the buffer / args


            //_messagesReceivedEvent(this, *args);
            _messagesReceivedEvent(*this, nullptr);
        }

        return S_OK;
    }


    //MIDIIN_CALLBACK_FUNC_DECL(MidiBidirectionalEndpointConnection::MidiInCallback)
    //{
    //    std::cout << __FUNCTION__ << " message(s) received" << std::endl;


    //    if (_messagesReceivedEvent)
    //    {
    //     //   auto args = winrt::make_self<MidiMessagesReceivedEventArgs>();

    //        // todo, set up the buffer / args


    //        //_messagesReceivedEvent(this, *args);
    //        _messagesReceivedEvent(*this, nullptr);
    //    }


    //    // check if we have any filters. If not, just copy all the data over

    //    // if we do have filters, check filter strategy


    //    // intent is to have one UMP at a time, but we will treat this as potentially multiple to allow for that in the future

    //    // read timestamp

    //    // read first byte of message to determine how many MIDI words to read

    //    // evaluate message

    //    // if passed eval, add it to the queue of incoming messages for this endpoint

    //}


    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Midi2::IMidiMessageClientFilter> MidiBidirectionalEndpointConnection::Filters()
    {
        throw hresult_not_implemented();
    }
    winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy MidiBidirectionalEndpointConnection::FilterStrategy()
    {
        throw hresult_not_implemented();
    }
    void MidiBidirectionalEndpointConnection::FilterStrategy(winrt::Windows::Devices::Midi2::MidiMessageClientFilterStrategy const& value)
    {
        throw hresult_not_implemented();
    }

    uint32_t MidiBidirectionalEndpointConnection::ReceiveBuffer(winrt::Windows::Devices::Midi2::MidiMessageBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive)
    {
        throw hresult_not_implemented();
    }

    uint32_t MidiBidirectionalEndpointConnection::SendBuffer(winrt::Windows::Devices::Midi2::MidiMessageBuffer const& buffer, uint32_t byteOffsetInBuffer, uint32_t maxBytesToSend)
    {
        throw hresult_not_implemented();

        //buffer.data();

        // return the number of bytes written



    }

    // just an alignment test. Alignment is normally on 64 bit boundaries.
    // so need to ensure that when we're reading/writing x-proc.
    // this *should* work without packing as we always have that 64 bit timestamp
    // which starts off the struct. But there's a potential compatibility issue
    // with the byte buffers and with reading 32 bit words. So we really need to 
    // align on 32 bit boundaries.
    // Complication: https://devblogs.microsoft.com/oldnewthing/20200103-00/?p=103290
    // Also complication: MIDL 3.0 doesn't allow packing
    //
    // the declspec is for things like arrays. The pack controls size. Both are needed.
    //
#define MIDIWORDALIGNEDSTRUCT __declspec(align(sizeof(uint32_t))) struct

#pragma pack(push, 4)
    MIDIWORDALIGNEDSTRUCT testump32
    {
        uint64_t timestamp;
        uint32_t word0;
    };
    MIDIWORDALIGNEDSTRUCT testump64
    {
        uint64_t timestamp;
        uint32_t word0;
        uint32_t word1;
    };
    MIDIWORDALIGNEDSTRUCT testump96
    {
        uint64_t timestamp;
        uint32_t word0;
        uint32_t word1;
        uint32_t word2;
    };
    MIDIWORDALIGNEDSTRUCT testump128
    {
        uint64_t timestamp;
        uint32_t word0;
        uint32_t word1;
        uint32_t word2;
        uint32_t word3;
    };
#pragma pack(pop)

    void MidiBidirectionalEndpointConnection::TEMPTEST_SendUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
    {
        std::cout << __FUNCTION__ << " sending message to service" << std::endl;

        try
        {
            if (_bidiEndpoint)
            {
                // TODO: Problem: the WinRT MidiUmp32 type is 16 bytes. It should be only 12. There's an additional 4 bytes in it.
                testump32 smallUmp32;
                smallUmp32.timestamp = ump.Timestamp;
                smallUmp32.word0 = ump.Word0;

                //testump64 smallUmp64;
                //smallUmp64.timestamp = ump.Timestamp;
                //smallUmp64.word0 = ump.Word0;
                //smallUmp64.word1 = ump.Word0;

                //testump96 smallUmp96;
                //smallUmp96.timestamp = ump.Timestamp;
                //smallUmp96.word0 = ump.Word0;
                //smallUmp96.word1 = ump.Word0;
                //smallUmp96.word2 = ump.Word0;

                //testump128 smallUmp128;
                //smallUmp128.timestamp = ump.Timestamp;
                //smallUmp128.word0 = ump.Word0;
                //smallUmp128.word1 = ump.Word0;
                //smallUmp128.word2 = ump.Word0;
                //smallUmp128.word3 = ump.Word0;

                size_t umpSize = sizeof(winrt::Windows::Devices::Midi2::MidiUmp32);

                std::cout << "size of WinRT ump32 :  " << umpSize << " ( " << sizeof(MidiUmp32) * 8 -64 << "bits +64 for timestamp )" << std::endl;
                std::cout << "size of WinRT ump64 :  " << sizeof(MidiUmp64) << " ( " << sizeof(MidiUmp64) * 8 - 64 << "bits +64 for timestamp )" << std::endl;
                std::cout << "size of WinRT ump96 :  " << sizeof(MidiUmp96) << " ( " << sizeof(MidiUmp96) * 8 - 64 << "bits +64 for timestamp )" << std::endl;
                std::cout << "size of WinRT ump128:  " << sizeof(MidiUmp128) << " ( " << sizeof(MidiUmp128) * 8 - 64 << "bits +64 for timestamp )" << std::endl;



                std::cout << "size of packed ump32:  " << sizeof(testump32) << " ( " << sizeof(testump32) * 8 - 64 << "bits +64 for timestamp )" << std::endl;
                std::cout << "size of packed ump64:  " << sizeof(testump64) << " ( " << sizeof(testump64) * 8 - 64 << "bits +64 for timestamp )" << std::endl;
                std::cout << "size of packed ump96:  " << sizeof(testump96) << " ( " << sizeof(testump96) * 8 - 64 << "bits +64 for timestamp )" << std::endl;
                std::cout << "size of packed ump128: " << sizeof(testump128) << " ( " << sizeof(testump128) * 8 - 64 << "bits +64 for timestamp )" << std::endl;


                _bidiEndpoint->SendMidiMessage((void*)&ump, umpSize, 0);
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return;
        }

        std::cout << __FUNCTION__ << " message sent" << std::endl;

    }



    // internal method to start listening for incoming messages, enable processing outgoing messages, etc.
    bool MidiBidirectionalEndpointConnection::Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection)
    {

        // wire up our callback

//        InternalMidiInCallbackFunc callbackFunc = std::bind(&MidiBidirectionalEndpointConnection::MidiInCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

        //deviceConnection->RegisteredListeners.push_back(callbackFunc);

        _bidiEndpoint = deviceConnection->BidirectionalConnection;

        _isConnected = true;

        // Begin temporary code =============================================

        //MidiUmp32 ump;
        //std::cout << __FUNCTION__ << " sending message" << std::endl;

        //umpEndpoint->SendMidiMessage((PVOID)&ump, sizeof(MidiUmp32), 0);

        //std::cout << __FUNCTION__ << " waiting for callback" << std::endl;

        //int timeoutCounter = 1000;
        //while (!_tempMessagesReceivedFlag && timeoutCounter > 0)
        //{
        //    Sleep(10);

        //    timeoutCounter--;
        //}

        //if (!_tempMessagesReceivedFlag)
        //{
        //    std::cout << __FUNCTION__ << " Failed. Waited for messages but they never arrived. " << std::endl;
        //}
        // End Temporary test code ===================================================

        return true;
    }

}
