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
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMessageResultFromHRESULT(HRESULT hr)
    {
        midi2::MidiSendMessageResults result{ 0 };

        if (SUCCEEDED(hr))
        {
            result |= midi2::MidiSendMessageResults::Succeeded;
        }
        else
        {
            result |= midi2::MidiSendMessageResults::Failed;
        }

        switch (hr)
        {
        case HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER):
            result |= midi2::MidiSendMessageResults::BufferFull;
            break;

        default:
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
    midi2::MidiSendMessageResults MidiEndpointConnection::SendMessageRaw(
        winrt::com_ptr<IMidiBidirectional> endpoint,
        void* data,
        uint8_t sizeInBytes,
        internal::MidiTimestamp timestamp)
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
            TraceLoggingUInt8(sizeInBytes, MIDI_SDK_TRACE_MESSAGE_SIZE_BYTES_FIELD)
        );
#endif

        try
        {
            if (!m_isOpen)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Send failed. Endpoint is not open", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. Endpoint is not open\n");

                // return failure if we're not open
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::EndpointConnectionClosedOrInvalid;
            }

            if (timestamp != 0 && timestamp > internal::GetCurrentMidiTimestamp() + m_maxAllowedTimestampOffset)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Send failed. Timestamp exceeds maximum future scheduling offset", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                    TraceLoggingUInt64(timestamp, MIDI_SDK_TRACE_MESSAGE_TIMESTAMP_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. Timestamp exceeds maximum future scheduling offset\n");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::TimestampOutOfRange;
            }


            if (endpoint != nullptr)
            {
                MessageOptionFlags flags{ MessageOptionFlags::MessageOptionFlags_None };

                if (m_connectionSettings.WaitForEndpointReceiptOnSend())
                {
                    flags = MessageOptionFlags::MessageOptionFlags_WaitForSendComplete;
                }

                auto hr = endpoint->SendMidiMessage(flags, data, sizeInBytes, timestamp);

                
                if (FAILED(hr))
                {
                    LOG_IF_FAILED(hr);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Send failed. SendMidiMessage returned a failure code", MIDI_SDK_TRACE_MESSAGE_FIELD),
                        TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                        TraceLoggingHResult(hr, MIDI_SDK_TRACE_HRESULT_FIELD)
                    );

                    OutputDebugString(L"MIDI App SDK: Send failed. SendMidiMessage returned a failure code\n");
                }

                return SendMessageResultFromHRESULT(hr);
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
                    TraceLoggingWideString(L"Send failed. Endpoint is nullptr", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. Endpoint is nullptr\n");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Send failed. hresult error sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed. HRESULT error sending message. Is the service running?\n");

            return midi2::MidiSendMessageResults::Failed;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendUmpInternal(
        winrt::com_ptr<IMidiBidirectional> endpoint,
        midi2::IMidiUniversalPacket const& ump)
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
            TraceLoggingUInt32(static_cast<uint32_t>(ump.PacketType()), MIDI_SDK_TRACE_UMP_TYPE_FIELD)
        );
#endif

        try
        {
            uint8_t umpDataSize{};

            auto umpDataPointer = GetUmpDataPointer(ump, umpDataSize);

            if (umpDataPointer == nullptr)
            {
                LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Send failed. UMP data pointer is nullptr", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
                );

                OutputDebugString(L"MIDI App SDK: Send failed. UMP data pointer is nullptr\n");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageOther;
            }

            return SendMessageRaw(endpoint, umpDataPointer, umpDataSize, ump.Timestamp());

        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Send failed. HRESULT error sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed. HRESULT error sending message. Is the service running?\n");

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
                TraceLoggingWideString(L"Send failed. Exception sending message. Is the service running?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingWideString(m_endpointDeviceId.c_str(), MIDI_SDK_TRACE_ENDPOINT_DEVICE_ID_FIELD)
            );

            OutputDebugString(L"MIDI App SDK: Send failed. Exception sending message. Is the service running?\n");

            return midi2::MidiSendMessageResults::Failed;
        }
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


}
