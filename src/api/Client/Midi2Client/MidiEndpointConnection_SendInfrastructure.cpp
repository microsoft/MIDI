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
            result |= midi2::MidiSendMessageResults::BufferFull;
            break;

        default:
            result |= midi2::MidiSendMessageResults::Other;
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
                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::EndpointConnectionClosedOrInvalid;
            }

            if (timestamp != 0 && timestamp > internal::GetCurrentMidiTimestamp() + m_maxAllowedTimestampOffset)
            {
                internal::LogGeneralError(__FUNCTION__, L"Timestamp exceeds maximum future scheduling offset");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::TimestampOutOfRange;
            }

            if (endpoint != nullptr)
            {
                auto hr = endpoint->SendMidiMessage(data, sizeInBytes, timestamp);

                return SendMessageResultFromHRESULT(hr);
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::EndpointConnectionClosedOrInvalid;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            // TOD: Handle other hresults like buffer full
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
        }
    }


    _Use_decl_annotations_
    midi2::MidiSendMessageResults MidiEndpointConnection::SendUmpInternal(
        winrt::com_ptr<IMidiBiDi> endpoint,
        midi2::IMidiUniversalPacket const& ump)
    {
        internal::LogInfo(__FUNCTION__, L"Sending message internal");
        try
        {
            uint8_t umpDataSize{};

            auto umpDataPointer = GetUmpDataPointer(ump, umpDataSize);

            if (umpDataPointer == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint data pointer is nullptr");

                return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::InvalidMessageOther;
            }

            return SendMessageRaw(endpoint, umpDataPointer, umpDataSize, ump.Timestamp());

        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            // TODO: handle buffer full and other expected hresults
            return midi2::MidiSendMessageResults::Failed | midi2::MidiSendMessageResults::Other;
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
