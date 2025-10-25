// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiEndpointConnection.h"

#include "ump_iterator.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{

    void CopyMidiMessageStructDataToWordVector(_In_ std::vector<uint32_t>& destination, _In_ MidiMessageStruct const& message)
    {
        auto messageWordCount = internal::GetUmpLengthInMidiWordsFromFirstWord(message.Word0);

        // have to hoop-jump here a little because this function needs the parameter
        // to be const to allow calling from the winrt functions which themselves
        // take in a const parameter. Don't worry, we won't change the pointer :P
        auto nonConst = const_cast<MidiMessageStruct*>(&message);
        uint32_t* messageData = reinterpret_cast<uint32_t*>(nonConst);

        for (int i = 0; i < messageWordCount; i++)
        {
            destination.push_back(*(messageData + i));
        }

    }



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

        // validate that we're not sending more than the allowed number of words
        if (static_cast<uint32_t>(byteCount / sizeof(uint32_t)) > static_cast<uint32_t>(GetSupportedMaxMidiWordsPerTransmission()))
        {
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::TransmissionWordCountExceeded;
        }


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
                    TraceLoggingWideString(L"Send failed. Buffer smaller than provided offset + byteLength (first check)", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. Buffer smaller than provided offset + byteLength (first check)\n");

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
                    TraceLoggingWideString(L"Send failed. Buffer smaller than provided offset + byteLength (second check)", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. Buffer smaller than provided offset + byteLength (second check)\n");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
            }


            // endianness becomes a concern so we need to make sure we treat this as words
            uint8_t* byteDataPointer = dataPointer + byteOffset;



            // TODO: Validate the data using the ump iterator

            // TODO: Validate length isn't > largest transmission size








            auto result = SendMessageRaw(
                m_endpointTransport, 
                static_cast<void*>(byteDataPointer), 
                byteCount, 
                timestamp);

            if (SendMessageFailed(result))
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Send failed. SendMessageRaw failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. SendMessageRaw failed\n");

                return result;
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
                TraceLoggingWideString(L"Send failed. Unable to get buffer", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed. Unable to get buffer\n");

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

        // we don't support a buffer size of 0 or a null buffer
        if (words == nullptr)
        {
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageOther;
        }

        // this copy is unfortunate, but the WinRT type doesn't support pointer access 
        // to the data and iiterable is not guaranteed to be contiguous. If you need 
        // faster sending without this allocation, use the COM extension
        std::vector<uint32_t> wordList{};
        std::copy(begin(words), end(words), std::back_inserter(wordList));

        // validate that we're not sending more than the allowed number of words
        if (static_cast<uint64_t>(wordList.size()) > static_cast<uint64_t>(GetSupportedMaxMidiWordsPerTransmission()))
        {
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::TransmissionWordCountExceeded;
        }


        // validate that the messages are complete. If you don't want this step, you
        // need to use the COM extension
        internal::UmpBufferIterator bufferIterator(wordList.data(), static_cast<uint32_t>(wordList.size()));

        for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
        {
            if (!it.CurrentMessageSeemsComplete())
            {
                LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Incomplete messages in send buffer", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Incomplete messages in send buffer.\n");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }
        }


        // SendMessageRaw supports multiple messages. However, it doesn't do any
        // validation of the messages, so we had to do that above.
        auto sendMessageResult = SendMessageRaw(
            m_endpointTransport, 
            static_cast<void*>(wordList.data()),
            static_cast<uint32_t>(wordList.size() * sizeof(uint32_t)), 
            timestamp);

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
                TraceLoggingWideString(L"Send failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed.\n");

            return sendMessageResult;
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

        // we don't support a buffer size of 0 or a null buffer
        if (words.size() == 0 || wordCount == 0)
        {
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageOther;
        }

        // validate that we're not sending more than the allowed number of words
        if (wordCount > static_cast<uint32_t>(GetSupportedMaxMidiWordsPerTransmission()))
        {
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::TransmissionWordCountExceeded;
        }


        // validate that the messages are complete. If you don't want this step, you
        // need to use the COM extension
        internal::UmpBufferIterator bufferIterator(
            (uint32_t*)(words.data() + startIndex), 
            static_cast<uint32_t>(wordCount));

        for (auto it = bufferIterator.begin(); it < bufferIterator.end(); ++it)
        {
            if (!it.CurrentMessageSeemsComplete())
            {
                LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Incomplete messages in send buffer", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Incomplete messages in send buffer.\n");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }
        }

        // SendMessageRaw supports multiple messages. However, it doesn't do any
        // validation of the messages, so we had to do that above.
        auto sendMessageResult = SendMessageRaw(
            m_endpointTransport,
            (PVOID)((words.data() + startIndex)),
            static_cast<uint32_t>((wordCount) * sizeof(uint32_t)),
            timestamp);

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
                TraceLoggingWideString(L"Send failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed.\n");

            return sendMessageResult;
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

        std::vector<uint32_t> wordList;

        for (auto const& message : messages)
        {
            CopyMidiMessageStructDataToWordVector(wordList, message);
        }

        // we've already validated the message integrity as part of the copy, so we can just send

        auto sendMessageResult = SendMessageRaw(
            m_endpointTransport,
            static_cast<void*>(wordList.data()),
            static_cast<uint32_t>(wordList.size() * sizeof(uint32_t)),
            timestamp);

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
                TraceLoggingWideString(L"Send failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed.\n");

            return sendMessageResult;
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

        std::vector<uint32_t> wordList;

        for (auto i = startIndex; i < startIndex + messageCount; i++)
        {
            CopyMidiMessageStructDataToWordVector(wordList, messages[i]);
        }

        // we've already validated the message integrity as part of the copy, so we can just send

        auto sendMessageResult = SendMessageRaw(
            m_endpointTransport,
            static_cast<void*>(wordList.data()),
            static_cast<uint32_t>(wordList.size() * sizeof(uint32_t)),
            timestamp);

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
                TraceLoggingWideString(L"Send failed", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed.\n");

            return sendMessageResult;
        }

        return midi2::MidiSendMessageResults::Succeeded;

    }


    // packets include timestamps, so this function must send each message individually
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
                    TraceLoggingWideString(L"Send failed.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. Index out of bounds\n");

                // if any fail, we return immediately.

                return result;
            }
        }

        return midi2::MidiSendMessageResults::Succeeded;

    }

}
