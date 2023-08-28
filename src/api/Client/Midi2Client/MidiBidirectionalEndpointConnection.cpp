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


    // TODO: Refactor this common code (Output and BiDi, Input and BiDi) into another class









    // Callback handler from the Midi Service endpoint abstraction

    IFACEMETHODIMP MidiBidirectionalEndpointConnection::Callback(_In_ PVOID Data, _In_ UINT Size, _In_ LONGLONG Timestamp)
    {
        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<MidiMessageReceivedEventArgs>(Data, Size, Timestamp);

            // we failed to create the event args
            if (args == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs");

                return E_FAIL;
            }

            bool skipMainMessageReceivedEvent = false;
            bool skipFurtherListeners = false;

            // If any listeners are hooked up, use them

            if (m_messageListeners && m_messageListeners.Size() > 0)
            {
                // loop through listeners
                for (const auto& listener : m_messageListeners)
                {
                    // TODO: this is synchronous by design, but that requires the client to not block
                    listener.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);

                    // if the listener has told us to skip further listeners, effectively 
                    // removing this message from the queue, then break out of the loop
                    if (skipFurtherListeners) break;
                }
            }

            // if the main message received event is hooked up, and we're not skipping it, use it
            if (m_messageReceivedEvent && !skipMainMessageReceivedEvent)
            {
                m_messageReceivedEvent(*this, *args);
            }

            return S_OK;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception calling message handlers", ex);

            return E_FAIL;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmpBuffer(
        _In_ const internal::MidiTimestamp timestamp, 
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& buffer, 
        _In_ const uint32_t byteOffset, 
        _In_ const uint32_t byteLength)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }

            // make sure we're sending only a single UMP
            uint32_t sizeInWords = byteLength / sizeof(uint32_t);

            if (!internal::IsValidSingleUmpWordCount(sizeInWords))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for a single UMP", sizeInWords, timestamp);

                //throw hresult_invalid_argument();
                return false;
            }

            Windows::Foundation::IMemoryBufferReference bufferReference = buffer.CreateReference();

            auto interop = bufferReference.as<IMemoryBufferByteAccess>();

            uint8_t* dataPointer{};
            uint32_t dataSize{};
            winrt::check_hresult(interop->GetBuffer(&dataPointer, &dataSize));

            // make sure we're not going to spin past the end of the buffer
            if (byteOffset + byteLength > bufferReference.Capacity())
            {
                // TODO: Log

                internal::LogGeneralError(__FUNCTION__, L"Buffer smaller than provided offset + byteLength");

                return false;
            }


            // send the ump

            return m_messageSenderHelper.SendMessageRaw(m_endpointInterface, (void*)(dataPointer + byteOffset), byteLength, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__,  L"hresult exception sending message", ex);

            return false;
        }
    }


    // sends a single UMP's worth of words
    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmpWordArray(
        _In_ internal::MidiTimestamp const timestamp, 
        _In_ array_view<uint32_t const> words, 
        _In_ uint32_t const wordCount)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }


            if (!internal::IsValidSingleUmpWordCount(wordCount))
            {
                //throw hresult_invalid_argument();
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for a single UMP", wordCount, timestamp);

                return false;
            }

            if (internal::GetUmpLengthInMidiWordsFromFirstWord(words[0]) != wordCount)
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", wordCount, timestamp);

                return false;
            }


            if (m_endpointInterface)
            {
                auto umpDataSize = (uint32_t)(sizeof(uint32_t) * wordCount);

                // if the service goes down, this will fail

                return m_messageSenderHelper.SendMessageRaw(m_endpointInterface, (void*)words.data(), umpDataSize, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            return false;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const timestamp, 
        _In_ uint32_t const word0)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP32_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP32_WORD_COUNT, timestamp);
                return false;
            }


            if (m_endpointInterface)
            {
                auto umpByteCount = (uint32_t)(sizeof(internal::PackedUmp32));

                // if the service goes down, this will fail

                return m_messageSenderHelper.SendMessageRaw(m_endpointInterface, (void*)&word0, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            return false;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint32_t const word0, 
        _In_ uint32_t const word1)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP64_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP64_WORD_COUNT, timestamp);
                return false;
            }


            if (m_endpointInterface)
            {
                auto umpByteCount = (uint32_t)(sizeof(internal::PackedUmp64));
                internal::PackedUmp64 ump{};

                ump.word0 = word0;
                ump.word1 = word1;

                // if the service goes down, this will fail

                return m_messageSenderHelper.SendMessageRaw(m_endpointInterface, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            return false;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint32_t const word0,
        _In_ uint32_t const word1,
        _In_ uint32_t const word2)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP96_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP96_WORD_COUNT, timestamp);
                return false;
            }


            if (m_endpointInterface)
            {
                auto umpByteCount = (uint32_t)(sizeof(internal::PackedUmp96));
                internal::PackedUmp96 ump{};

                ump.word0 = word0;
                ump.word1 = word1;
                ump.word2 = word2;

                // if the service goes down, this will fail

                return m_messageSenderHelper.SendMessageRaw(m_endpointInterface, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            return false;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint32_t const word0,
        _In_ uint32_t const word1,
        _In_ uint32_t const word2,
        _In_ uint32_t const word3)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP128_WORD_COUNT)
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP128_WORD_COUNT, timestamp);
                return false;
            }


            if (m_endpointInterface)
            {
                auto umpByteCount = (uint32_t)(sizeof(internal::PackedUmp128));
                internal::PackedUmp128 ump{};

                ump.word0 = word0;
                ump.word1 = word1;
                ump.word2 = word2;
                ump.word3 = word3;

                // if the service goes down, this will fail

                return m_messageSenderHelper.SendMessageRaw(m_endpointInterface, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            return false;
        }
    }


    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::SendUmp(
        _In_ winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return false;
            }


            if (m_endpointInterface)
            {
                return m_messageSenderHelper.SendUmp(m_endpointInterface, ump);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message. Service may be unavailable", ex);

            return false;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::ActivateMidiStream(
        _In_ com_ptr<IMidiAbstraction> serviceAbstraction, 
        _In_ const IID& iid, 
        _Out_ void** iface)
    {
        try
        {
            winrt::check_hresult(serviceAbstraction->Activate(iid, iface));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception activating stream. Service may be unavailable", ex);

            return false;
        }
    }
    
    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::InternalInitialize(
        _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction)
    {
        try
        {
            WINRT_ASSERT(!DeviceId().empty());
            WINRT_ASSERT(serviceAbstraction != nullptr);

            m_serviceAbstraction = serviceAbstraction;

            // TODO: Read any settings we need for this endpoint

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint.", ex);

            return false;
        }
    }


    _Success_(return == true)
    bool MidiBidirectionalEndpointConnection::Open()
    {
        if (!m_isOpen)
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiBiDi), (void**)&m_endpointInterface))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                return false;
            }

            if (m_endpointInterface != nullptr)
            {
                try
                {
                    DWORD mmcssTaskId{};  // TODO: Does this need to be session-wide? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

                    winrt::check_hresult(m_endpointInterface->Initialize(
                        (LPCWSTR)(DeviceId().c_str()),
                        &mmcssTaskId,
                        (IMidiCallback*)(this)
                    ));

                    m_isOpen = true;


                    // TODO: Send discovery messages using app provided settings and user settings read from the property store
                    // These get fired off here quickly so we can return. The listener is responsible for catching them.







                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_endpointInterface = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" Endpoint interface is nullptr");

                return false;

            }


        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }


    MidiBidirectionalEndpointConnection::~MidiBidirectionalEndpointConnection()
    {
        if (m_endpointInterface != nullptr)
        {
            m_endpointInterface->Cleanup();
        }

        m_isOpen = false;

        // TODO: any event cleanup?
    }

}
