// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"




namespace winrt::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesBuffer(
        internal::MidiTimestamp timestamp,
        foundation::IMemoryBuffer const& buffer,
        uint32_t byteOffset,
        uint32_t byteCount) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        try
        {
            winrt::Windows::Foundation::IMemoryBufferReference bufferReference = buffer.CreateReference();

            auto interop = bufferReference.as<IMemoryBufferByteAccess>();

            bool continueProcessing = true;
            uint8_t* dataPointer{};
            uint32_t dataSize{ };

            if (SUCCEEDED(interop->GetBuffer(&dataPointer, &dataSize)))
            {
                // make sure we're not going to spin past the end of the buffer
                if (byteOffset + byteCount > bufferReference.Capacity())
                {
                    internal::LogGeneralError(__FUNCTION__, L"Buffer smaller than provided offset + byteLength");

                    return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::DataIndexOutOfRange;
                }


                // endianness becomes a concern so we need to make sure we treat this as words
                //uint32_t* wordDataPointer = (uint32_t*)(dataPointer + byteOffset);
                uint8_t* byteDataPointer = dataPointer + byteOffset;
                uint32_t bytesProcessed{ 0 };

                while (continueProcessing)
                {
                    if ((byteOffset + bytesProcessed) < dataSize &&
                        (bytesProcessed + sizeof(uint32_t) <= byteCount))
                    {
                        uint8_t bytesInThisMessage = internal::GetUmpLengthInBytesFromFirstWord(*(uint32_t*)byteDataPointer);

                        if (bytesProcessed + bytesInThisMessage <= byteCount)
                        {
                            auto result = SendMessageRaw(m_endpointAbstraction, (void*)(byteDataPointer), bytesInThisMessage, timestamp);

                            if (SendMessageFailed(result))
                            {
                                internal::LogGeneralError(__FUNCTION__, L"SendMessageRaw failed");
                                return result;
                            }
                        }
                        else
                        {
                            internal::LogGeneralError(__FUNCTION__, L"Ran out of data");
                            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::MessageListPartiallyProcessed;
                        }

                        bytesProcessed += bytesInThisMessage;

                        byteDataPointer += bytesInThisMessage;
                    }
                    else
                    {
                        continueProcessing = false;
                    }
                }

                if (bytesProcessed != byteCount)
                {
                    internal::LogGeneralError(__FUNCTION__, L"Ran out of data");
                    return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::MessageListPartiallyProcessed;

                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to get buffer");

                return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
            }

            return midi2::MidiSendMessageResult::Succeeded;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
        }
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesWordList(
        internal::MidiTimestamp timestamp,
        collections::IVectorView<uint32_t> const& words) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        // iterate through words based on the message type in the next word

        // we keep reusing this struct
        internal::PackedUmp128 messageData{};

        //uint32_t index = 0;

        //// TODO: Iteration via index would likely be faster if we just use a array_view, IVector, or IVectorView instead of IIterable.

        //// or maybe we can use a winrt::array_view somehow and get conversion from std?
        //// https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/std-cpp-data-types
        //// https://devblogs.microsoft.com/oldnewthing/20200205-00/?p=103398/

        //while (index < totalWords)
        //{

        //}


        auto iter = words.First();


        while (iter.HasCurrent())
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(iter.Current());

            if (messageWordCount >= 1)
            {
                messageData.word0 = iter.Current();
            }

            if (messageWordCount >= 2)
            {
                if (iter.MoveNext())
                {
                    messageData.word1 = iter.Current();
                }
                else
                {
                    internal::LogGeneralError(__FUNCTION__, L"Failed to send message. Message type indicates 2+ words, but we ran out of data.");
                    return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::DataIndexOutOfRange;
                }
            }

            if (messageWordCount >= 3)
            {
                if (iter.MoveNext())
                {
                    messageData.word2 = iter.Current();
                }
                else
                {
                    internal::LogGeneralError(__FUNCTION__, L"Failed to send message. Message type indicates 3+ words, but we ran out of data.");
                    return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::DataIndexOutOfRange;
                }

            }

            if (messageWordCount == 4)
            {
                if (iter.MoveNext())
                {
                    messageData.word3 = iter.Current();
                }
                else
                {
                    internal::LogGeneralError(__FUNCTION__, L"Failed to send message. Message type indicates 4 words, but we ran out of data.");
                    return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::DataIndexOutOfRange;
                }
            }

            auto sendMessageResult = SendMessageRaw(m_endpointAbstraction, &messageData, messageWordCount * sizeof(uint32_t), timestamp);

            if (SendMessageFailed(sendMessageResult))
            {
                // failed. Log and return. We fail on first problem.
                internal::LogGeneralError(__FUNCTION__, L"Failed to send message");

                return sendMessageResult;

            }

            iter.MoveNext();
        }

        return midi2::MidiSendMessageResult::Succeeded;

    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesWordArray(
        internal::MidiTimestamp timestamp,
        winrt::array_view<uint32_t> words) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");


        UNREFERENCED_PARAMETER(timestamp);
        UNREFERENCED_PARAMETER(words);

        // TODO: Implement SendMultipleMessagesWordArray


        return midi2::MidiSendMessageResult::Failed | midi2::MidiSendMessageResult::Other;
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesStructList(
        internal::MidiTimestamp timestamp,
        collections::IVectorView<MidiMessageStruct> const& messages) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        for (auto const& message : messages)
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(message.Word0);
            auto sendMessageResult = SendMessageRaw(m_endpointAbstraction, (VOID*)&message, messageWordCount * sizeof(uint32_t), timestamp);

            if (SendMessageFailed(sendMessageResult))
            {
                // failed. Log and return. We fail on first problem.
                internal::LogGeneralError(__FUNCTION__, L"Failed to send message");

                return sendMessageResult;
            }
        }

        return midi2::MidiSendMessageResult::Succeeded;
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesStructArray(
        internal::MidiTimestamp timestamp,
        winrt::array_view<MidiMessageStruct> messages) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        for (auto const& message : messages)
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(message.Word0);
            auto sendMessageResult = SendMessageRaw(m_endpointAbstraction, (VOID*)&message, messageWordCount * sizeof(uint32_t), timestamp);

            if (SendMessageFailed(sendMessageResult))
            {
                // failed. Log and return. We fail on first problem.
                internal::LogGeneralError(__FUNCTION__, L"Failed to send message");

                return sendMessageResult;
            }
        }

        return midi2::MidiSendMessageResult::Succeeded;

    }




    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesPacketList(
        collections::IVectorView<IMidiUniversalPacket> const& messages) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Sending multiple message packet list");

        for (auto const& ump : messages)
        {
            auto result = SendUmpInternal(m_endpointAbstraction, ump);

            if (!MidiEndpointConnection::SendMessageSucceeded(result))
            {
                // if any fail, we return immediately.

                return result;
            }
        }

        return midi2::MidiSendMessageResult::Succeeded;

    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResult MidiEndpointConnection::SendMultipleMessagesPacketArray(
        winrt::array_view<IMidiUniversalPacket> messages) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Enter");


        for (auto const& ump : messages)
        {
            auto result = SendUmpInternal(m_endpointAbstraction, ump);

            if (!MidiEndpointConnection::SendMessageSucceeded(result))
            {
                // if any fail, we return immediately.

                return result;
            }
        }

        return midi2::MidiSendMessageResult::Succeeded;

    }




}
