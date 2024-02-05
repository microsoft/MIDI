// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"
#include "MidiEndpointConnection.g.cpp"




namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageResultFromHRESULT(HRESULT hr)
    {
        midi2::MidiSendMessageResult result{ 0 };

        if (SUCCEEDED(hr))
        {
            result |= midi2::MidiSendMessageResult::Succeeded;
        }
        else
        {
            result |= midi2::MidiSendMessageResult::Failed;
        }

        switch (hr)
        {
        //case HR_S_MIDI_SENDMSG_IMMEDIATE:
        //    result |= midi2::MidiSendMessageResult::SentImmediately;
        //    break;

        //case HR_S_MIDI_SENDMSG_SCHEDULED:
        //    result |= midi2::MidiSendMessageResult::Scheduled;
        //    break;

        //case HR_S_MIDI_SENDMSG_SYSEX_PARKED:
        //    result |= midi2::MidiSendMessageResult::SystemExclusiveParked;
        //    break;

        //case HR_E_MIDI_SENDMSG_BUFFER_FULL:
        //    result |= midi2::MidiSendMessageResult::BufferFull;
        //    break;

        //case HR_E_MIDI_SENDMSG_SCHEDULER_QUEUE_FULL:
        //    result |= midi2::MidiSendMessageResult::SchedulerQueueFull;
        //    break;

        //case HR_E_MIDI_SENDMSG_INVALID_MESSAGE:
        //    result |= midi2::MidiSendMessageResult::InvalidMessageOther;
        //    break;

        case HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER):
            result |= midi2::MidiSendMessageResult::BufferFull;
            break;

        default:
            result |= midi2::MidiSendMessageResult::Other;
            break;
        }

        return result;
    }


    _Use_decl_annotations_
    bool MidiEndpointConnection::ValidateUmp(uint32_t word0, uint8_t wordCount) noexcept
    {
        if (!internal::IsValidSingleUmpWordCount(wordCount))
            return false;

        if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != wordCount)
            return false;

        return true;
    }



    _Use_decl_annotations_
    HRESULT MidiEndpointConnection::Callback(PVOID data, UINT size, LONGLONG timestamp, LONGLONG)
    {
        internal::LogInfo(__FUNCTION__, L"Message Received");

        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<midi2::implementation::MidiMessageReceivedEventArgs>(data, size, timestamp);

            // we failed to create the event args
            if (args == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs");

                return E_FAIL;
            }

            bool skipMainMessageReceivedEvent = false;
            bool skipFurtherListeners = false;

            // If any listeners are hooked up, use them

            if (m_messageProcessingPlugins && m_messageProcessingPlugins.Size() > 0)
            {
                // loop through listeners
                for (const auto& plugin : m_messageProcessingPlugins)
                {
                    // This is synchronous by design, but that requires the listener (and the client app which sinks any event) to not block

                    plugin.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);

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



    
    _Use_decl_annotations_
    bool MidiEndpointConnection::InternalInitialize(
        winrt::guid sessionId,
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        winrt::guid const connectionId,
        winrt::hstring const endpointDeviceId, 
        midi2::MidiEndpointConnectionOptions options
    )
    {
        internal::LogInfo(__FUNCTION__, L"Internal Initialize ");

        try
        {
            m_sessionId = sessionId;
            m_connectionId = connectionId;

            m_endpointDeviceId = endpointDeviceId;

            //WINRT_ASSERT(!m_endpointDeviceId.empty());
            //WINRT_ASSERT(serviceAbstraction != nullptr);

            m_serviceAbstraction = serviceAbstraction;
            
            m_options = options;

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint.", ex);

            return false;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointConnection::Open()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Open ");

        if (!IsOpen())
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiBiDi), (void**)&m_endpointAbstraction))
            {
                OutputDebugString(__FUNCTION__ L" coult not activate MIDI stream");

                internal::LogGeneralError(__FUNCTION__, L"Could not activate MIDI Stream");

                return false;
            }

            if (m_endpointAbstraction != nullptr)
            {
                try
                {
                    InitializePlugins();

                    DWORD mmcssTaskId{};  
                    ABSTRACTIONCREATIONPARAMS abstractionCreationParams{ MidiDataFormat_UMP };

                    winrt::check_hresult(m_endpointAbstraction->Initialize(
                        (LPCWSTR)(EndpointDeviceId().c_str()),
                        &abstractionCreationParams,
                        &mmcssTaskId,
                        (IMidiCallback*)(this),
                        0,
                        m_sessionId
                    ));

                    // provide a copy to the output logic
                    m_isOpen = true;

                    CallOnConnectionOpenedOnPlugins();

                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L"hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_endpointAbstraction = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint interface is nullptr");

                return false;
            }
        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }



    void MidiEndpointConnection::Close()
    {
        internal::LogInfo(__FUNCTION__, L"Connection Close");

        if (m_closeHasBeenCalled) return;

        try
        {
            CleanupPlugins();

            if (m_endpointAbstraction != nullptr)
            {
                m_endpointAbstraction->Cleanup();
                m_endpointAbstraction = nullptr;
            }

            // output is also input, so don't call cleanup
            m_endpointAbstraction = nullptr;

            m_isOpen = false;

            // TODO: any event cleanup?

            m_closeHasBeenCalled = true;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception on close");
        }
    }

    MidiEndpointConnection::~MidiEndpointConnection()
    {
        OutputDebugString(__FUNCTION__ L"");

        if (!m_closeHasBeenCalled)
        {
            Close();
        }
    }




    void MidiEndpointConnection::InitializePlugins() noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        internal::LogInfo(__FUNCTION__, L"Initializing message processing plugins");

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Initialize(*this);
            }
            catch (...)
            {
                internal::LogGeneralError(__FUNCTION__, L"Exception initializing plugins.");

            }
        }
    }


    void MidiEndpointConnection::CallOnConnectionOpenedOnPlugins() noexcept
    {
        OutputDebugString(__FUNCTION__ L"");

        internal::LogInfo(__FUNCTION__, L"Notifying message processing plugins that the connection is opened");

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.OnEndpointConnectionOpened();
            }
            catch (...)
            {
                internal::LogGeneralError(__FUNCTION__, L"Exception calling Open on plugins.");
            }
        }
    }

    void MidiEndpointConnection::CleanupPlugins() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Cleaning up plugins");

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Cleanup();
            }
            catch (...)
            {
                internal::LogGeneralError(__FUNCTION__, L"Exception cleaning up plugins.");
            }
        }
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageRaw(
        winrt::com_ptr<IMidiBiDi> endpoint,
        void* data,
        uint8_t sizeInBytes,
        internal::MidiTimestamp timestamp)
    {
        internal::LogInfo(__FUNCTION__, L"Sending message raw");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }

            if (timestamp != 0 && timestamp > ::Windows::Devices::Midi2::Internal::Shared::GetCurrentMidiTimestamp() + m_maxAllowedTimestampOffset)
            {
                internal::LogGeneralError(__FUNCTION__, L"Timestamp exceeds maximum future scheduling offset");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::TimestampOutOfRange;
            }

            if (endpoint != nullptr)
            {
                auto hr = endpoint->SendMidiMessage(data, sizeInBytes, timestamp);

                return SendMessageResultFromHRESULT(hr);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            // TOD: Handle other hresults like buffer full
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }




    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageStruct(
        internal::MidiTimestamp timestamp,
        midi2::MidiMessageStruct const& message,
        uint8_t wordCount) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message struct");

        if (!ValidateUmp(message.Word0, wordCount))
        {
            internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for this UMP", wordCount, timestamp);

            return midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
        }

        auto byteLength = (uint8_t)(wordCount * sizeof(uint32_t));

        return SendMessageRaw(m_endpointAbstraction, (void*)(&message), byteLength, timestamp);
    }


    _Use_decl_annotations_
    void* MidiEndpointConnection::GetUmpDataPointer(
        midi2::IMidiUniversalPacket const& ump,
        uint8_t& dataSizeOut)
    {
        void* umpDataPointer{};
        dataSizeOut = 0;

        switch (ump.PacketType())
        {
        case midi2::MidiPacketType::UniversalMidiPacket32:
            dataSizeOut = (uint8_t)sizeof(internal::PackedUmp32);
            umpDataPointer = ump.as<implementation::MidiMessage32>()->GetInternalUmpDataPointer();
            break;

        case midi2::MidiPacketType::UniversalMidiPacket64:
            dataSizeOut = (uint8_t)sizeof(internal::PackedUmp64);
            umpDataPointer = ump.as<implementation::MidiMessage64>()->GetInternalUmpDataPointer();
            break;

        case midi2::MidiPacketType::UniversalMidiPacket96:
            dataSizeOut = (uint8_t)sizeof(internal::PackedUmp96);
            umpDataPointer = ump.as<implementation::MidiMessage96>()->GetInternalUmpDataPointer();
            break;

        case midi2::MidiPacketType::UniversalMidiPacket128:
            dataSizeOut = (uint8_t)sizeof(internal::PackedUmp128);
            umpDataPointer = ump.as<implementation::MidiMessage128>()->GetInternalUmpDataPointer();
            break;
        }

        return umpDataPointer;
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendUmpInternal(
        winrt::com_ptr<IMidiBiDi> endpoint,
        midi2::IMidiUniversalPacket const& ump)
    {
        internal::LogInfo(__FUNCTION__, L"Sending message internal");
        try
        {
            if (endpoint != nullptr)
            {
                uint8_t umpDataSize{};

                auto umpDataPointer = GetUmpDataPointer(ump, umpDataSize);

                if (umpDataPointer == nullptr)
                {
                    internal::LogGeneralError(__FUNCTION__, L"endpoint data pointer is nullptr");

                    return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageOther;
                }

                return SendMessageRaw(endpoint, umpDataPointer, umpDataSize, ump.Timestamp());
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageBuffer(
        const internal::MidiTimestamp timestamp,
        winrt::Windows::Foundation::IMemoryBuffer const& buffer,
        const uint32_t byteOffset,
        const uint8_t byteLength) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message buffer");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }

            // make sure we're sending only a single UMP
            uint32_t sizeInWords = byteLength / sizeof(uint32_t);


            if (!internal::IsValidSingleUmpWordCount(sizeInWords))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for a single UMP", sizeInWords, timestamp);

                //throw hresult_invalid_argument();
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
            }

            winrt::Windows::Foundation::IMemoryBufferReference bufferReference = buffer.CreateReference();

            auto interop = bufferReference.as<IMemoryBufferByteAccess>();

            uint8_t* dataPointer{};
            uint32_t dataSize{};
            winrt::check_hresult(interop->GetBuffer(&dataPointer, &dataSize));

            // make sure we're not going to spin past the end of the buffer
            if (byteOffset + byteLength > bufferReference.Capacity())
            {
                internal::LogGeneralError(__FUNCTION__, L"Buffer smaller than provided offset + byteLength");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::DataIndexOutOfRange;
            }


            // send the ump

            return SendMessageRaw(m_endpointAbstraction, (void*)(dataPointer + byteOffset), byteLength, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageWordArray(
        internal::MidiTimestamp const timestamp,
        winrt::array_view<uint32_t const> words,
        uint32_t const startIndex,
        uint8_t const wordCount
        ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message word array");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }

            // check for out-of-bounds first
            if (startIndex + wordCount > words.size())
            {
                internal::LogGeneralError(__FUNCTION__, L"Array start index + word count is >= array size");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::DataIndexOutOfRange;
            }

            // check if the message type is correct for the word count
            if (wordCount < 1 || !ValidateUmp(words[startIndex], wordCount))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for this UMP", wordCount, timestamp);

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
            }

            if (m_endpointAbstraction)
            {
                auto umpDataSize = (uint8_t)(sizeof(uint32_t) * wordCount);

                // if the service goes down, this will fail

                return SendMessageRaw(m_endpointAbstraction, (void*)(words.data() + startIndex), umpDataSize, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageWords(
        internal::MidiTimestamp const timestamp,
        uint32_t const word0) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message words (1)");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP32_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP32_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
            }


            if (m_endpointAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp32));

                // if the service goes down, this will fail

                return SendMessageRaw(m_endpointAbstraction, (void*)&word0, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageWords(
        internal::MidiTimestamp const timestamp,
        uint32_t const word0,
        uint32_t const word1) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message words (2)");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP64_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP64_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
            }


            if (m_endpointAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp64));
                internal::PackedUmp64 ump{};

                ump.word0 = word0;
                ump.word1 = word1;

                // if the service goes down, this will fail

                return SendMessageRaw(m_endpointAbstraction, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageWords(
        internal::MidiTimestamp const timestamp,
        uint32_t const word0,
        uint32_t const word1,
        uint32_t const word2) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message words (3)");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP96_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP96_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
            }


            if (m_endpointAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp96));
                internal::PackedUmp96 ump{};

                ump.word0 = word0;
                ump.word1 = word1;
                ump.word2 = word2;

                // if the service goes down, this will fail

                return SendMessageRaw(m_endpointAbstraction, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessageWords(
        internal::MidiTimestamp const timestamp,
        uint32_t const word0,
        uint32_t const word1,
        uint32_t const word2,
        uint32_t const word3) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message words (4)");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP128_WORD_COUNT)
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP128_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::InvalidMessageTypeForWordCount;
            }


            if (m_endpointAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp128));
                internal::PackedUmp128 ump{};

                ump.word0 = word0;
                ump.word1 = word1;
                ump.word2 = word2;
                ump.word3 = word3;

                // if the service goes down, this will fail

                return SendMessageRaw(m_endpointAbstraction, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMessagePacket(
        midi2::IMidiUniversalPacket const& message) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message packet");

        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }


            if (m_endpointAbstraction)
            {
                return SendUmpInternal(m_endpointAbstraction, message);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message. Service may be unavailable", ex);


            // todo: handle buffer full and similar messages
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }


    _Use_decl_annotations_
    bool MidiEndpointConnection::ActivateMidiStream(
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        const IID& iid,
        void** iface) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Activating MIDI Stream");

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




}
