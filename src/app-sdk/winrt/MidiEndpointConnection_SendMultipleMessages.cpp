// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMultipleMessagesBuffer(
        internal::MidiTimestamp const timestamp,
        uint32_t const byteOffset,
        uint32_t const byteCount,
        foundation::IMemoryBuffer const& buffer
        ) noexcept
    {
#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
            TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
            TraceLoggingUInt32(byteOffset, "byte offset"),
            TraceLoggingUInt32(byteCount, "byte count")
        );
#endif
        winrt::Windows::Foundation::IMemoryBufferReference bufferReference = buffer.CreateReference();

        auto interop = bufferReference.as<IMemoryBufferByteAccess>();

        uint8_t* dataPointer{};
        uint32_t dataSize{ };

        if (SUCCEEDED(interop->GetBuffer(&dataPointer, &dataSize)))
        {
            if (byteOffset + byteCount > dataSize)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Buffer smaller than provided offset + byteLength (first check)", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
            }


            // make sure we're not going to spin past the end of the buffer
            if (byteOffset + byteCount > bufferReference.Capacity())
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Buffer smaller than provided offset + byteLength (second check)", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
            }


            // endianness becomes a concern so we need to make sure we treat this as words
            uint8_t* byteDataPointer = dataPointer + byteOffset;
            uint32_t bytesProcessed{ 0 };

            while (true)
            {
                uint8_t bytesInThisMessage = internal::GetUmpLengthInBytesFromFirstWord(*(uint32_t*)byteDataPointer);

                if (bytesProcessed + bytesInThisMessage <= byteCount)
                {
                    auto result = SendMessageRaw(m_endpointTransport, (void*)(byteDataPointer), bytesInThisMessage, timestamp);

                    if (SendMessageFailed(result))
                    {
                        LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"SendMessageRaw failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                        );

                        return result;
                    }
                }
                else
                {
                    LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Ran out of data when sending multiple messages", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                    );


                    auto returnValue = midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange ;

                    if (bytesProcessed > 0)
                    {
                        returnValue |= midi2::MidiSendMessageResults::MessageListPartiallyProcessed;
                    }

                    return returnValue;
                }

                bytesProcessed += bytesInThisMessage;
                byteDataPointer += bytesInThisMessage;

                if (bytesProcessed >= byteCount)
                {
                    break;
                }
            }

            if (bytesProcessed != byteCount)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Finished, but bytes processed != supplied count", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::MessageListPartiallyProcessed;
            }
        }
        else
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Unable to get buffer", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
        }

        return midi2::MidiSendMessageResults::Succeeded;
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMultipleMessagesWordList(
        internal::MidiTimestamp const timestamp,
        collections::IIterable<uint32_t> const& words) noexcept
    {
#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
            TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD)
        );
#endif
        uint32_t i{ 0 };

        uint32_t messageWords[4]{ 0,0,0,0 };   // we reuse this storage

        auto iter = words.First();

        while (iter.HasCurrent())
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(iter.Current());

            for (uint32_t j = 0; j < messageWordCount; j++)
            {
                //messageWords[j] = words.GetAt(i + j);

                messageWords[j] = iter.Current();
                iter.MoveNext();
            }

            auto sendMessageResult = SendMessageRaw(m_endpointTransport, (VOID*)(messageWords), messageWordCount * sizeof(uint32_t), timestamp);

            if (SendMessageFailed(sendMessageResult))
            {
                // failed. Log and return. We fail on first problem.
                LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to send message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                if (i > 0)
                {
                    sendMessageResult |= midi2::MidiSendMessageResults::MessageListPartiallyProcessed;
                }

                return sendMessageResult;
            }

            i += messageWordCount;
        }

        return midi2::MidiSendMessageResults::Succeeded;

    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMultipleMessagesWordArray(
        internal::MidiTimestamp const timestamp,
        uint32_t const startIndex,
        uint32_t const wordCount,
        winrt::array_view<uint32_t const> words) noexcept
    {
#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
            TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
            TraceLoggingUInt64(startIndex, "start index"),
            TraceLoggingUInt64(wordCount, "word count")
        );
#endif

        uint32_t i{ startIndex };
        uint32_t wordsSent{ 0 };

        while (i < words.size() && wordsSent < wordCount)
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(words[i]);

            if (i + messageWordCount <= words.size())
            {
                auto sendMessageResult = SendMessageRaw(m_endpointTransport, (VOID*)(words.data() + i), messageWordCount * sizeof(uint32_t), timestamp);

                if (SendMessageFailed(sendMessageResult))
                {
                    // failed. Log and return. We fail on first problem.
                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Failed to send message.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                    );

                    if (i > 0)
                    {
                        sendMessageResult |= midi2::MidiSendMessageResults::MessageListPartiallyProcessed;
                    }

                    return sendMessageResult;
                }

                i += messageWordCount;
                wordsSent += messageWordCount;
            }
            else
            {
                // failed. Log and return. We fail on first problem.
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to send message. Index out of bounds", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;

            }
        }

        return midi2::MidiSendMessageResults::Succeeded;
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMultipleMessagesStructList(
        internal::MidiTimestamp const timestamp,
        collections::IIterable<MidiMessageStruct> const& messages) noexcept
    {
#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
            TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD)
        );
#endif
        for (auto const& message : messages)
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(message.Word0);
            auto sendMessageResult = SendMessageRaw(m_endpointTransport, (VOID*)&message, messageWordCount * sizeof(uint32_t), timestamp);

            if (SendMessageFailed(sendMessageResult))
            {
                // failed. Log and return. We fail on first problem.
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to send message.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                return sendMessageResult;
            }
        }

        return midi2::MidiSendMessageResults::Succeeded;
    }



    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMultipleMessagesStructArray(
        internal::MidiTimestamp const timestamp,
        uint32_t const startIndex,
        uint32_t const messageCount,
        winrt::array_view<MidiMessageStruct const> messages) noexcept
    {
#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
            TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
            TraceLoggingUInt32(startIndex, "start index"),
            TraceLoggingUInt32(startIndex, "message count")
        );
#endif

        auto iterator = messages.begin() + startIndex;
        auto endpoint = iterator + messageCount;

        while (iterator < messages.end() && iterator < endpoint)
        {
            auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(iterator->Word0);
            auto sendMessageResult = SendMessageRaw(m_endpointTransport, (VOID*)iterator, messageWordCount * sizeof(uint32_t), timestamp);

            if (SendMessageFailed(sendMessageResult))
            {
                // failed. Log and return. We fail on first problem.
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to send message.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );


                return sendMessageResult;
            }

            iterator++;
        }

        return midi2::MidiSendMessageResults::Succeeded;

    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMultipleMessagesPacketList(
        collections::IIterable<IMidiUniversalPacket> const& messages) noexcept
    {
#ifdef _DEBUG
        // performance-critical function, so only trace when in a debug build
        TraceLoggingWrite(
            Midi2SdkTelemetryProvider::Provider(),
            MIDI_SDK_TRACE_EVENT_INFO,
            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_INFO),
            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
            TraceLoggingWideString(L"Enter", MIDI_SDK_TRACE_MESSAGE_FIELD),
            TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
            TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
        );
#endif
        for (auto const& ump : messages)
        {
            auto result = SendUmpInternal(m_endpointTransport, ump);

            if (!MidiEndpointConnection::SendMessageSucceeded(result))
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Failed to send message.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                // if any fail, we return immediately.

                return result;
            }
        }

        return midi2::MidiSendMessageResults::Succeeded;

    }

}
