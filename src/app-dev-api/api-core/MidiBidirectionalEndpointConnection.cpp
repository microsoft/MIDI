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


    void MidiBidirectionalEndpointConnection::TEMPTEST_SendUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
    {
        std::cout << __FUNCTION__ << " sending message to service" << std::endl;

        try
        {
            if (_bidiEndpoint)
            {
                _bidiEndpoint->SendMidiMessage((void*)&ump, sizeof(winrt::Windows::Devices::Midi2::MidiUmp32), 0);
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
