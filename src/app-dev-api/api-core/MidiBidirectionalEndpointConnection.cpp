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


namespace winrt::Windows::Devices::Midi2::implementation
{
    // Callback handler from the Midi Service endpoint abstraction

    IFACEMETHODIMP MidiBidirectionalEndpointConnection::Callback(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Timestamp)
    {   
        if (_messageReceivedEvent)
        {
            auto args = _messageReceiverHelper.CreateEventArgsFromCallbackParams(Data, Size, Timestamp);

            if (args != nullptr)
            {
                _messageReceivedEvent(*this, args);
            }
            else
            {
                return E_FAIL;
            }
        }

        return S_OK;
    }

    // sends a single UMP's worth of words
    bool MidiBidirectionalEndpointConnection::SendUmpWords(uint64_t timestamp, array_view<uint32_t const> words, uint32_t wordCount)
    {
        try
        {
            if (!internal::IsValidSingleUmpWordCount(wordCount))
            {
                return false;
            }


            if (_bidiEndpoint)
            {
                auto umpDataSize = (uint32_t)(sizeof(uint32_t) * wordCount);

                // if the service goes down, this will fail

                return _messageSenderHelper.SendMessageRaw(_bidiEndpoint, (void*)words.data(), umpDataSize, timestamp);
            }
            else
            {
                OutputDebugString(L"" __FUNCTION__ " _bidiEndpoint is nullptr");
                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            //std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
            //std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            //std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }
    }



    bool MidiBidirectionalEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        try
        {
            if (_bidiEndpoint)
            {
                return _messageSenderHelper.SendUmp(_bidiEndpoint, ump);
            }
            else
            {
                OutputDebugString(L"" __FUNCTION__ " _bidiEndpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            //std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
            //std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            //std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            OutputDebugString(L"" __FUNCTION__ " hresult exception sending message. Service may be unavailable.");

            return false;
        }
    }


    // internal method to start listening for incoming messages, enable processing outgoing messages, etc.
    // TODO: Change signature of this
    bool MidiBidirectionalEndpointConnection::InternalStart(std::shared_ptr<internal::InternalMidiDeviceConnection> deviceConnection)
    {

        _bidiEndpoint = deviceConnection->BidirectionalConnection;

        _isConnected = true;

        return true;
    }

    MidiBidirectionalEndpointConnection::~MidiBidirectionalEndpointConnection()
    {
        if (_bidiEndpoint != nullptr)
        {
            _bidiEndpoint->Cleanup();
        }

        _isConnected = false;

        // TODO: any event cleanup?
    }



}
