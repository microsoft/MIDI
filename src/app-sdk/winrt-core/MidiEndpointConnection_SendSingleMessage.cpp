// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageStruct(
        internal::MidiTimestamp const timestamp,
        uint8_t const wordCount,
        midi2::MidiMessageStruct const& message
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
            TraceLoggingUInt8(wordCount, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD)
            );
#endif

        if (!ValidateUmp(message.Word0, wordCount))
        {
            LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP data");   // this also generates a fallback error with file and line number info

            if (LogMessageDataValidationErrorDetails())
            {
                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Word count is incorrect for this UMP", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                    TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                    TraceLoggingUInt8(wordCount, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD),
                    TraceLoggingUInt32(message.Word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
                );
            }

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
            TraceLoggingUInt8(byteCount, "byte count")
        );
#endif


        try
        {
            // make sure we're sending only a single UMP
            uint32_t sizeInWords = byteCount / sizeof(uint32_t);


            if (!internal::IsValidSingleUmpWordCount(sizeInWords))
            {
                LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP data");   // this also generates a fallback error with file and line number info

                if (LogMessageDataValidationErrorDetails())
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Word count is incorrect for this UMP", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                        TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                        TraceLoggingUInt32(sizeInWords, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD),
                        TraceLoggingUInt32(byteOffset, "byte offset"),
                        TraceLoggingUInt8(byteCount, MIDI_SDK_TRACE_MESSAGE_SIZE_BYTES_FIELD)
                    );
                }

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
                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Buffer smaller than provided offset + byteLength", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
                    );

                    return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
                }

                // send the ump
                return SendMessageRaw(m_endpointAbstraction, (void*)(dataPointer + byteOffset), byteCount, timestamp);
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
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
                );

                return midi2::MidiSendMessageResults::Failed;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            //internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
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
            TraceLoggingUInt8(wordCount, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD)
        );
#endif

        try
        {
            // check for out-of-bounds first
            if (startIndex + wordCount > words.size())
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Array start index + word count is > array size", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD)
                );

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::DataIndexOutOfRange;
            }

            // check if the message type is correct for the word count
            if (!ValidateUmp(words[startIndex], wordCount))
            {
                LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP data");   // this also generates a fallback error with file and line number info

                if (LogMessageDataValidationErrorDetails())
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Word count is incorrect for this UMP", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                        TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                        TraceLoggingUInt32(startIndex, "start index"),
                        TraceLoggingUInt8(wordCount, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD)
                    );
                }

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }

            auto umpDataSize = (uint8_t)(sizeof(uint32_t) * wordCount);

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)(words.data() + startIndex), umpDataSize, timestamp);

        }
        catch (winrt::hresult_error const& ex)
        {
            //internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
        internal::MidiTimestamp const timestamp,
        uint32_t const word0) noexcept
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
            TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
        );
#endif

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP32_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP32 data");   // this also generates a fallback error with file and line number info

                if (LogMessageDataValidationErrorDetails())
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Word count is incorrect for UMP32 message type", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                        TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                        TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
                    );
                }

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageTypeForWordCount;
            }


            auto umpByteCount = (uint8_t)(sizeof(internal::PackedUmp32));

            // if the service goes down, this will fail

            return SendMessageRaw(m_endpointAbstraction, (void*)&word0, umpByteCount, timestamp);
        }
        catch (winrt::hresult_error const& ex)
        {
            //internal::LogHresultError(__FUNCTION__, L" hresult exception sending message", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1) noexcept
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
            TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
        );
#endif

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP64_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP64 data");   // this also generates a fallback error with file and line number info

                if (LogMessageDataValidationErrorDetails())
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Word count is incorrect for UMP64 message type", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                        TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                        TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
                    );
                }

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
            //internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
        }
    }

    _Use_decl_annotations_
        midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessageWords(
            internal::MidiTimestamp const timestamp,
            uint32_t const word0,
            uint32_t const word1,
            uint32_t const word2) noexcept
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
            TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
            TraceLoggingUInt8(3, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD)
        );
#endif

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP96_WORD_COUNT)
            {
                // mismatch between the message type and the number of words
                LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP96 data");   // this also generates a fallback error with file and line number info

                if (LogMessageDataValidationErrorDetails())
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Word count is incorrect for UMP96 message type", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                        TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                        TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
                    );
                }

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
            //internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
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
            TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
            TraceLoggingUInt8(4, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD)
        );
#endif

        try
        {
            if (internal::GetUmpLengthInMidiWordsFromFirstWord(word0) != UMP128_WORD_COUNT)
            {
                LOG_IF_FAILED_MSG(E_INVALIDARG, "Invalid UMP128 data");   // this also generates a fallback error with file and line number info

                if (LogMessageDataValidationErrorDetails())
                {
                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Word count is incorrect for UMP128 message type", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingGuid(m_connectionId, MIDI_SDK_TRACE_CONNECTION_ID_FIELD),
                        TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
                        TraceLoggingUInt32(word0, MIDI_SDK_TRACE_MESSAGE_WORD0_FIELD)
                    );
                }

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
            //internal::LogHresultError(__FUNCTION__, L"hresult exception sending message", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );


            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
        }
    }


    _Use_decl_annotations_
        midi2::MidiSendMessageResults MidiEndpointConnection::SendSingleMessagePacket(
            midi2::IMidiUniversalPacket const& message) noexcept
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
            TraceLoggingUInt64(ump.Timestamp(), MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD),
            TraceLoggingUInt32(ump.MessageType(), MIDI_SDK_TRACE_UMP_TYPE)
        );
#endif

        try
        {
            return SendUmpInternal(m_endpointAbstraction, message);

        }
        catch (winrt::hresult_error const& ex)
        {
            //internal::LogHresultError(__FUNCTION__, L"hresult exception sending message. Service may be unavailable", ex, m_endpointDeviceId);
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception sending message. Service may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );


            // todo: handle buffer full and similar messages
            return midi2::MidiSendMessageResults::Failed;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            return midi2::MidiSendMessageResults::Failed;
        }
    }
}
