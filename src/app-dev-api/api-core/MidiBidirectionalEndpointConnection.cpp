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
        //    auto args = _messageReceiverHelper.CreateMessageEventArgsFromCallbackParams(Data, Size, Timestamp);

            auto args = winrt::make_self<MidiMessageReceivedEventArgs>(Data, Size, Timestamp);

            if (args != nullptr)
            {
                _messageReceivedEvent(*this, *args);
            }
            else
            {
                return E_FAIL;
            }
        }

        return S_OK;
    }


    bool MidiBidirectionalEndpointConnection::SendUmpBuffer(uint64_t timestamp, winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffset, uint32_t byteLength)
    {
        try
        {
            // make sure we're sending only a single UMP
            if (!internal::IsValidSingleUmpWordCount(byteLength / sizeof(uint32_t)))
            {
                return false;
            }

            Windows::Foundation::IMemoryBufferReference bufferReference = buffer.CreateReference();

            auto interop = bufferReference.as<IMemoryBufferByteAccess>();

            uint8_t* dataPointer;
            uint32_t dataSize;
            winrt::check_hresult(interop->GetBuffer(&dataPointer, &dataSize));

            // make sure we're not going to spin past the end of the buffer
            if (byteOffset + byteLength > bufferReference.Capacity())
            {
                return false;
            }


            // send the ump

            return _messageSenderHelper.SendMessageRaw(_endpointInterface, (void*)(dataPointer + byteOffset), byteLength, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {

            //std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
            //std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            //std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }
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


            if (_endpointInterface)
            {
                auto umpDataSize = (uint32_t)(sizeof(uint32_t) * wordCount);

                // if the service goes down, this will fail

                return _messageSenderHelper.SendMessageRaw(_endpointInterface, (void*)words.data(), umpDataSize, timestamp);
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


    bool MidiBidirectionalEndpointConnection::SendUmp32Words(uint64_t timestamp, uint32_t word0)
    {
        throw hresult_not_implemented();
    }
    bool MidiBidirectionalEndpointConnection::SendUmp64Words(uint64_t timestamp, uint32_t word0, uint32_t word1)
    {
        throw hresult_not_implemented();
    }
    bool MidiBidirectionalEndpointConnection::SendUmp96Words(uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2)
    {
        throw hresult_not_implemented();
    }
    bool MidiBidirectionalEndpointConnection::SendUmp128Words(uint64_t timestamp, uint32_t word0, uint32_t word1, uint32_t word2, uint32_t word3)
    {
        throw hresult_not_implemented();
    }



    bool MidiBidirectionalEndpointConnection::SendUmp(winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        try
        {
            if (_endpointInterface)
            {
                return _messageSenderHelper.SendUmp(_endpointInterface, ump);
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


    bool MidiBidirectionalEndpointConnection::ActivateMidiStream(com_ptr<IMidiAbstraction> serviceAbstraction, const IID& iid, void** iface)
    {
        try
        {
            winrt::check_hresult(serviceAbstraction->Activate(iid, iface));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            //std::cout << __FUNCTION__ << ": hresult exception on Service Abstraction Activate (service may not be installed or running or endpoint type is wrong)" << std::endl;
            //std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            //std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }
    }
    
    // internal method to start listening for incoming messages, enable processing outgoing messages, etc.
    // TODO: Change signature of this
    bool MidiBidirectionalEndpointConnection::InternalStart(winrt::com_ptr<IMidiAbstraction> serviceAbstraction)
    {
        WINRT_ASSERT(!DeviceId().empty());
        WINRT_ASSERT(serviceAbstraction != nullptr);

        DWORD mmcssTaskId;  // TODO: Does this need to be session-wise? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

        // Activate the endpoint for this device. Will fail if the device is not a BiDi device
        if (!ActivateMidiStream(serviceAbstraction, __uuidof(IMidiBiDi), (void**)&_endpointInterface))
        {
            return false;
        }

        try
        {
            // TODO: Need to handle the output only case which has no callback
            winrt::check_hresult(_endpointInterface->Initialize(
                (LPCWSTR)(DeviceId().c_str()),
                &mmcssTaskId,
                (IMidiCallback*)(this)
            ));

            _isConnected = true;

            return true;

        }
        catch (winrt::hresult_error const& ex)
        {
            //std::cout << __FUNCTION__ << " hresult exception on Initialize endpoint with callback" << std::endl;
            //std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            //std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            _endpointInterface = nullptr;

            return false;
        }

    }

    MidiBidirectionalEndpointConnection::~MidiBidirectionalEndpointConnection()
    {
        if (_endpointInterface != nullptr)
        {
            _endpointInterface->Cleanup();
        }

        _isConnected = false;

        // TODO: any event cleanup?
    }



}
