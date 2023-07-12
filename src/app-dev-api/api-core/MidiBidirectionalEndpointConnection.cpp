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

    IFACEMETHODIMP MidiBidirectionalEndpointConnection::Callback(PVOID Data, UINT Size, LONGLONG Timestamp)
    {
        std::cout << __FUNCTION__ << " message received in callback" << std::endl;
        std::cout << " - Data Size is " << std::dec << Size << std::endl;
        std::cout << " - Timestamp is " << std::hex << Timestamp << std::endl;

        // maybe checking this in the callback is overkill
        WINRT_ASSERT(internal::IsValidSingleUmpBufferSize(Size));
     

        // DEBUG. Walk the data and show some info
        byte mt = internal::GetUmpMessageTypeFromFirstWord(*(byte*)Data);
        std::cout << " - Message Type in data is " << std::hex << mt << std::endl;



        // check if we have any filters. If not, just copy all the data over

        // if we do have filters, check filter strategy


        // intent is to have one UMP at a time, but we will treat this as potentially multiple to allow for that in the future

        // read timestamp

        // read first byte of message to determine how many MIDI words to read

        // evaluate message

        // if passed eval, add it to the queue of incoming messages for this endpoint

        if (_messagesReceivedEvent)
        {
            auto args = winrt::make_self<MidiMessagesReceivedEventArgs>();

            // TODO: currently this supports only one message.
            if (Size == sizeof(internal::PackedUmp32))
            {

                auto ump = winrt::make_self<MidiUmp32>(Timestamp, Data);
                args->Ump(ump.as<IMidiUmp>());
            }
            else if (Size == sizeof(internal::PackedUmp64))
            {
                auto ump = winrt::make_self<MidiUmp64>(Timestamp, Data);
                args->Ump(ump.as<IMidiUmp>());
            }
            else if (Size == sizeof(internal::PackedUmp96))
            {
                auto ump = winrt::make_self<MidiUmp96>(Timestamp, Data);
                args->Ump(ump.as<IMidiUmp>());
            }
            else if (Size == sizeof(internal::PackedUmp128))
            {
                auto ump = winrt::make_self<MidiUmp128>(Timestamp, Data);
                args->Ump(ump.as<IMidiUmp>());
            }
            else
            {
                // shouldn't happen until we support more than one message
                return E_FAIL;
            }


 
            // todo, set up the buffer / args


            //_messagesReceivedEvent(*this, nullptr);
            _messagesReceivedEvent(*this, *args);
            //           _messagesReceivedEvent((IMidiInputConnection)(*this), args);
        }

        return S_OK;
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

    uint32_t MidiBidirectionalEndpointConnection::ReceiveBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffsetinBuffer, uint32_t maxBytesToReceive)
    {
        throw hresult_not_implemented();
    }


    uint32_t MidiBidirectionalEndpointConnection::SendBuffer(internal::MidiTimestamp, winrt::Windows::Foundation::IMemoryBuffer const& midiData, uint32_t byteOffset, uint32_t length)
    {
        //if ()

        throw hresult_not_implemented();
    }



    void MidiBidirectionalEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        // TODO: most or all of this logic needs to be factored out into the internal connection object

        std::cout << __FUNCTION__ << " sending message to service" << std::endl;

        try
        {
            if (_bidiEndpoint)
            {
                // https://gist.github.com/kennykerr/f1d941c2d26227abbf762481bcbd84d3

                Windows::Foundation::IMemoryBuffer umpDataBuffer = ump.RawData();

                auto ref = umpDataBuffer.CreateReference();

                auto interop = ref.as<IMemoryBufferByteAccess>();

                uint8_t* umpData{};
                uint32_t umpDataSize{};
                check_hresult(interop->GetBuffer(&umpData, &umpDataSize));

                WINRT_ASSERT(umpDataSize == sizeof(internal::PackedUmp32));

                // TEMP
                std::cout << "SendUmp. Bytes: " << std::endl;
                for (int i = 0; i < umpDataSize; i++)
                {
                    std::cout << std::hex << (int)(umpData[i]) << " ";
                }
                std::cout << std::endl;


                _bidiEndpoint->SendMidiMessage((void*)umpData, umpDataSize, ump.Timestamp());




                std::cout << __FUNCTION__ << " message sent" << std::endl;
            }
            else
            {
                std::cout << __FUNCTION__ << " _bidiEndpoint is nullptr" << std::endl;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
            std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return;
        }


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
