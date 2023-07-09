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
    void MidiBidirectionalEndpointConnection::InternalCallback(PVOID Data, UINT Size, LONGLONG Position)
    {
        // check if we have any filters. If not, just copy all the data over

        // if we do have filters, check filter strategy


        // intent is to have one UMP at a time, but we will treat this as potentially multiple to allow for that in the future

        // read timestamp

        // read first byte of message to determine how many MIDI words to read

        // evaluate message

        // if passed eval, add it to the queue of incoming messages for this endpoint

    }


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







    //IFACEMETHODIMP MidiBidirectionalEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Position)
    //{
    //    // TODO: process incoming messages / fire event

    //    std::cout << __FUNCTION__ << " loopback message received" << std::endl;

    //    //OutputDebugString(L"MidiBidirectionalEndpointConnection::Callback - loopback message received");

    //    _tempMessagesReceivedFlag = true;

    //    return S_OK;
    //}

    // internal method to start listening for incoming messages, enable processing outgoing messages, etc.
    bool MidiBidirectionalEndpointConnection::Start(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection)
    {


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
