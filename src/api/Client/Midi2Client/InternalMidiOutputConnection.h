// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

#include "MidiMessage32.h"
#include "MidiMessage32.h"
#include "MidiMessage32.h"
#include "MidiMessage32.h"



namespace Windows::Devices::Midi2::Internal
{
    template<typename TEndpointAbstraction>
    class InternalMidiOutputConnection
    {
    public:
        winrt::hstring OutputEndpointDeviceId() const noexcept { return m_outputDeviceId; }


        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessagePacket(
            _In_ midi2::IMidiUniversalPacket const& ump);


        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageStruct(
            _In_ internal::MidiTimestamp timestamp,
            _In_ midi2::MidiMessageStruct const& message,
            _In_ uint8_t wordCount);


        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0);

        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1);

        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2);

        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageWords(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ uint32_t const word0,
            _In_ uint32_t const word1,
            _In_ uint32_t const word2,
            _In_ uint32_t const word3);


        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageWordArray(
            _In_ internal::MidiTimestamp const timestamp,
            _In_ winrt::array_view<uint32_t const> words,
            _In_ uint32_t const startIndex,
            _In_ uint8_t const wordCount);



        _Success_(return == true)
        midi2::MidiSendMessageResult SendMessageBuffer(
            _In_ internal::MidiTimestamp timestamp,
            _In_ foundation::IMemoryBuffer const& buffer,
            _In_ uint32_t byteOffset,
            _In_ uint8_t byteLength);

    protected:
        winrt::hstring m_outputDeviceId{ };
        winrt::com_ptr<TEndpointAbstraction> m_outputAbstraction{ nullptr };

        void OutputIsOpen(_In_ bool const isOpen) noexcept { m_isOpen = isOpen; }
        bool OutputIsOpen() const noexcept { return m_isOpen; }

        bool ValidateUmp(_In_ uint32_t word0, _In_ uint8_t wordCount)
        {
            if (!internal::IsValidSingleUmpWordCount(wordCount))
                return false;

            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != wordCount)
                return false;

            return true;
        }

    private:

        bool m_isOpen{ false };

        _Success_(return == true)
        midi2::MidiSendMessageResult  SendUmpInternal(
            _In_ winrt::com_ptr<TEndpointAbstraction> endpoint, 
            _In_ midi2::IMidiUniversalPacket const& ump);

        _Success_(return == true)
        midi2::MidiSendMessageResult  SendMessageRaw(
            _In_ winrt::com_ptr<TEndpointAbstraction> endpoint, 
            _In_ void* data, 
            _In_ uint8_t sizeInBytes, 
            _In_ internal::MidiTimestamp timestamp);

        _Success_(return != nullptr)
        void* GetUmpDataPointer(
            _In_ midi2::IMidiUniversalPacket const& ump, 
            _Out_ uint8_t& dataSizeOut);

    };


    // implementation must be in header (or otherwise included) for template classes


    _Use_decl_annotations_
    template <typename TEndpointAbstraction>
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageRaw(
            winrt::com_ptr<TEndpointAbstraction> endpoint,
            void* data, 
            uint8_t sizeInBytes, 
            internal::MidiTimestamp timestamp)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (endpoint != nullptr)
            {

                winrt::check_hresult(endpoint->SendMidiMessage(data, sizeInBytes, timestamp));

                return midi2::MidiSendMessageResult::Success;
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            // TOD: Handle other hresults like buffer full
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }


    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageStruct(
        internal::MidiTimestamp timestamp,
        midi2::MidiMessageStruct const& message,
        uint8_t wordCount)
    {
        if (!ValidateUmp(message.Word0, wordCount))
        {
            internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for this UMP", wordCount, timestamp);

            return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
        }

        auto byteLength = (uint8_t)(wordCount * sizeof(uint32_t));

        // TODO: is enddianness ok here?
        return SendMessageRaw(m_outputAbstraction, (void*)(&message), byteLength, timestamp);
    }


    _Use_decl_annotations_
    template <typename TEndpointAbstraction>
    void* InternalMidiOutputConnection<TEndpointAbstraction>::GetUmpDataPointer(
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


    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendUmpInternal(
            winrt::com_ptr<TEndpointAbstraction> endpoint,
            midi2::IMidiUniversalPacket const& ump)
    {
        try
        {
            if (endpoint != nullptr)
            {
                uint8_t umpDataSize{};

                auto umpDataPointer = GetUmpDataPointer(ump, umpDataSize);

                if (umpDataPointer == nullptr)
                {
                    internal::LogGeneralError(__FUNCTION__, L"endpoint data pointer is nullptr");

                    return midi2::MidiSendMessageResult::ErrorInvalidMessageOther;
                }

                return SendMessageRaw(endpoint, umpDataPointer, umpDataSize, ump.Timestamp());
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }


    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageBuffer(
            const internal::MidiTimestamp timestamp,
            winrt::Windows::Foundation::IMemoryBuffer const& buffer,
            const uint32_t byteOffset,
            const uint8_t byteLength)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }

            // make sure we're sending only a single UMP
            uint32_t sizeInWords = byteLength / sizeof(uint32_t);


            if (!internal::IsValidSingleUmpWordCount(sizeInWords))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for a single UMP", sizeInWords, timestamp);

                //throw hresult_invalid_argument();
                return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
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

                midi2::MidiSendMessageResult::ErrorSendDataIndexOutOfRange;
            }


            // send the ump

            return SendMessageRaw(m_outputAbstraction, (void*)(dataPointer + byteOffset), byteLength, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }


    // sends a single UMP's worth of words
    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageWordArray(
            internal::MidiTimestamp const timestamp,
            winrt::array_view<uint32_t const> words,
            uint32_t const startIndex,
            uint8_t const wordCount
            )
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (wordCount < 1 || !ValidateUmp(words[0], wordCount))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for this UMP", wordCount, timestamp);

                return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
            }


            if (startIndex + wordCount > words.size())
            {
                internal::LogGeneralError(__FUNCTION__, L"Array start index + word count is > array size");

                midi2::MidiSendMessageResult::ErrorSendDataIndexOutOfRange;
            }


            if (m_outputAbstraction)
            {
                auto umpDataSize = (uint8_t)(sizeof(uint32_t) * wordCount);

                // if the service goes down, this will fail

                return SendMessageRaw(m_outputAbstraction, (void*)(words.data() + startIndex), umpDataSize, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }


    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP32_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP32_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
            }


            if (m_outputAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp32));

                // if the service goes down, this will fail

                return SendMessageRaw(m_outputAbstraction, (void*)&word0, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }

    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP64_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP64_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
            }


            if (m_outputAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp64));
                internal::PackedUmp64 ump{};

                ump.word0 = word0;
                ump.word1 = word1;

                // if the service goes down, this will fail

                return SendMessageRaw(m_outputAbstraction, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }

    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1,
            uint32_t const word2)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP96_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP96_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
            }


            if (m_outputAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp96));
                internal::PackedUmp96 ump{};

                ump.word0 = word0;
                ump.word1 = word1;
                ump.word2 = word2;

                // if the service goes down, this will fail

                return SendMessageRaw(m_outputAbstraction, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }

    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1,
            uint32_t const word2,
            uint32_t const word3)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP128_WORD_COUNT)
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP128_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResult::ErrorInvalidMessageTypeForWordCount;
            }


            if (m_outputAbstraction)
            {
                auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp128));
                internal::PackedUmp128 ump{};

                ump.word0 = word0;
                ump.word1 = word1;
                ump.word2 = word2;
                ump.word3 = word3;

                // if the service goes down, this will fail

                return SendMessageRaw(m_outputAbstraction, (void*)&ump, umpByteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }


    template <typename TEndpointAbstraction>
    _Use_decl_annotations_
    midi2::MidiSendMessageResult InternalMidiOutputConnection<TEndpointAbstraction>::SendMessagePacket(
            midi2::IMidiUniversalPacket const& message)
    {
        try
        {
            if (!m_isOpen)
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is not open. Did you forget to call Open()?");

                // return failure if we're not open
                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }


            if (m_outputAbstraction)
            {
                return SendUmpInternal(m_outputAbstraction, message);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Endpoint is nullptr");

                return midi2::MidiSendMessageResult::ErrorEndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message. Service may be unavailable", ex);


            // todo: handle buffer full and similar messages
            return midi2::MidiSendMessageResult::ErrorUnexpected;
        }
    }





}
