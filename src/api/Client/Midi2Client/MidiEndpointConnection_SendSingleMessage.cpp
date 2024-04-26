// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"




namespace MIDI_CPP_NAMESPACE::implementation
{

    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageStruct(
        internal::MidiTimestamp const timestamp,
        uint8_t const wordCount,
        midi2::MidiMessageStruct const& message
        ) noexcept
    {
        //internal::LogInfo(__FUNCTION__, L"Sending message struct");

        if (!ValidateUmp(message.Word0, wordCount))
        {
            internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for this UMP", wordCount, timestamp);

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
        }

        auto byteLength = (uint8_t)(wordCount * sizeof(uint32_t));

        return SendMessageRaw(m_endpointAbstraction, (void*)(&message), byteLength, timestamp);
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageBuffer(
        internal::MidiTimestamp const timestamp,
        uint32_t const byteOffset,
        uint8_t const byteCount,
        winrt::Windows::Foundation::IMemoryBuffer const& buffer
        ) noexcept
    {
        //internal::LogInfo(__FUNCTION__, L"Sending message buffer");

        try
        {
            // make sure we're sending only a single UMP
            uint32_t sizeInWords = byteCount / sizeof(uint32_t);


            if (!internal::IsValidSingleUmpWordCount(sizeInWords))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for a single UMP", sizeInWords, timestamp);

                //throw hresult_invalid_argument();
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }

            winrt::Windows::Foundation::IMemoryBufferReference bufferReference = buffer.CreateReference();

            auto interop = bufferReference.as<IMemoryBufferByteAccess>();

            uint8_t* dataPointer{};
            uint32_t dataSize{};

            if (SUCCEEDED(interop->GetBuffer(&dataPointer, &dataSize)))
            {
                // make sure we're not going to spin past the end of the buffer
                if (byteOffset + byteCount > bufferReference.Capacity())
                {
                    internal::LogGeneralError(__FUNCTION__, L"Buffer smaller than provided offset + byteLength");

                    return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
                }

                // send the ump
                return SendMessageRaw(m_endpointAbstraction, (void*)(dataPointer + byteOffset), byteCount, timestamp);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to get buffer");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWordArray(
            internal::MidiTimestamp const timestamp,
            uint32_t const startIndex,
            uint8_t const wordCount,
            winrt::array_view<uint32_t const> words
            ) noexcept
    {
        //internal::LogInfo(__FUNCTION__, L"Sending message word array");

        try
        {
            // check for out-of-bounds first
            if (startIndex + wordCount > words.size())
            {
                internal::LogGeneralError(__FUNCTION__, L"Array start index + word count is >= array size");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
            }

            // check if the message type is correct for the word count
            if (!ValidateUmp(words[startIndex], wordCount))
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for this UMP", wordCount, timestamp);

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }

            auto umpDataSize = (uint8_t)(sizeof(uint32_t) * wordCount);

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)(words.data() + startIndex), umpDataSize, timestamp);

        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
        internal::MidiTimestamp const timestamp,
        uint32_t const word0) noexcept
    {
        //internal::LogInfo(__FUNCTION__, L"Sending message words (1)");

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP32_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP32_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }


            auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp32));

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)&word0, umpByteCount, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1) noexcept
    {
        //internal::LogInfo(__FUNCTION__, L"Sending message words (2)");

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP64_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP64_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }


            auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp64));
            internal::PackedUmp64 ump{};

            ump.word0 = word0;
            ump.word1 = word1;

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)&ump, umpByteCount, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }

    _Use_decl_annotations_
        midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1,
            uint32_t const word2) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message words (3)");

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP96_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP96_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }


            auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp96));
            internal::PackedUmp96 ump{};

            ump.word0 = word0;
            ump.word1 = word1;
            ump.word2 = word2;

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)&ump, umpByteCount, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1,
            uint32_t const word2,
            uint32_t const word3) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message words (4)");

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP128_WORD_COUNT)
            {
                internal::LogUmpSizeValidationError(__FUNCTION__, L"Word count is incorrect for messageType", UMP128_WORD_COUNT, timestamp);
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }


            auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp128));
            internal::PackedUmp128 ump{};

            ump.word0 = word0;
            ump.word1 = word1;
            ump.word2 = word2;
            ump.word3 = word3;

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)&ump, umpByteCount, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }


    _Use_decl_annotations_
        midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessagePacket(
            midi2::IMidiUniversalPacket const& message) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending message packet");

        try
        {
            return SendUmpInternal(m_endpointAbstraction, message);

        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message. Service may be unavailable", ex);


            // todo: handle buffer full and similar messages
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception sending message");

            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }






}
