// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiMessageReceivedEventArgs.h"
#include "MidiMessageReceivedEventArgs.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    MidiMessageReceivedEventArgs::MidiMessageReceivedEventArgs(PVOID data, UINT sizeInBytes, internal::MidiTimestamp timestamp)
    {
        // this is making the assumption that what comes through here is somehow a valid UMP. We'll happily
        // copy over all the data, but what the client will receive when it grabs the UMP will be based
        // on the message type parsed from word0

        m_timestamp = timestamp;

        if (sizeInBytes <= (uint32_t)(MidiPacketType::UniversalMidiPacket128) * sizeof(uint32_t) && 
            sizeInBytes > 0 && 
            data != nullptr)
        {
            memcpy(&m_data, data, sizeInBytes);
        }
        else
        {
            // invalid UMP data. This should never happen, but just in case

            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Invalid UMP data", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingUInt32(sizeInBytes, MIDI_SDK_TRACE_MESSAGE_SIZE_BYTES_FIELD)
            );

        }
    }

    midi2::MidiPacketType MidiMessageReceivedEventArgs::PacketType() const noexcept
    {
        // a type 0 ump with no data is just the noop, and is perfectly valid, so we always run this

        return (MidiPacketType)(internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0));
    }

    midi2::MidiMessageType MidiMessageReceivedEventArgs::MessageType() const noexcept
    {
        return (MidiMessageType)(internal::GetUmpMessageTypeFromFirstWord(m_data.Word0));
    }


    midi2::IMidiUniversalPacket MidiMessageReceivedEventArgs::GetMessagePacket()
    {
        auto wordCount = GetValidMessageWordCount();

        if (wordCount == 1) // ump32
        {
            return winrt::make_self<implementation::MidiMessage32>(m_timestamp, m_data.Word0).as<midi2::IMidiUniversalPacket>();
        }
        else if (wordCount == 2)    // ump64
        {
            return winrt::make_self<implementation::MidiMessage64>(m_timestamp, m_data.Word0, m_data.Word1).as<midi2::IMidiUniversalPacket>();
        }
        else if (wordCount == 3) // ump96
        {
            return winrt::make_self<implementation::MidiMessage96>(m_timestamp, m_data.Word0, m_data.Word1, m_data.Word2).as<midi2::IMidiUniversalPacket>();
        }
        else if (wordCount == 4) // ump128
        {
            return winrt::make_self<implementation::MidiMessage128>(m_timestamp, m_data.Word0, m_data.Word1, m_data.Word2, m_data.Word3).as<midi2::IMidiUniversalPacket>();
        }
        else
        {
            // this should never happen

            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Invalid message packet type", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingUInt8(wordCount, MIDI_SDK_TRACE_MESSAGE_SIZE_WORDS_FIELD)
            );


            return nullptr;
        }
    }

    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::FillMessageStruct(
        midi2::MidiMessageStruct& message
        )
    {
        auto wordCount = GetValidMessageWordCount();

        message.Word0 = m_data.Word0;

        if (wordCount >= 2)
        {
            message.Word1 = m_data.Word1;
        }

        if (wordCount >= 3)
        {
            message.Word2 = m_data.Word2;
        }

        if (wordCount >= 4)
        {
            message.Word3 = m_data.Word3;
        }

        return wordCount;
    }

    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::FillWords(
        uint32_t& word0, 
        uint32_t& word1, 
        uint32_t& word2,
        uint32_t& word3)
    {
        auto wordCount = GetValidMessageWordCount();

        word0 = m_data.Word0;
        
        if (wordCount >= 2)
        {
            word1 = m_data.Word1;
        }

        if (wordCount >= 3)
        {
            word2 = m_data.Word2;
        }

        if (wordCount >= 4)
        {
            word3 = m_data.Word3;
        }

        return wordCount;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillMessage32(
        midi2::MidiMessage32 const& ump)
    {
        if (ump == nullptr)
        {
            WINRT_ASSERT(false);    // this will cause a break in debug builds

            LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"UMP parameter is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        if (PacketType() != MidiPacketType::UniversalMidiPacket32)
        {
            WINRT_ASSERT(false); // this will cause a break in debug builds

            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Incorrect UMP Packet type for Ump32", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiMessage32, midi2::MidiMessage32>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiPacketType::UniversalMidiPacket32) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillMessage64(
        midi2::MidiMessage64 const& ump)
    {
        if (ump == nullptr)
        {
            WINRT_ASSERT(false);    // this will cause a break in debug builds

            LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"UMP parameter is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );


            return false;
        }

        if (PacketType() != MidiPacketType::UniversalMidiPacket64)
        {
            WINRT_ASSERT(false); // this will cause a break in debug builds

            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Incorrect UMP Packet type for Ump64", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiMessage64, midi2::MidiMessage64>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiPacketType::UniversalMidiPacket64) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillMessage96(
        midi2::MidiMessage96 const& ump)
    {
        if (ump == nullptr)
        {
            WINRT_ASSERT(false);    // this will cause a break in debug builds

            LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"UMP parameter is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        if (PacketType() != MidiPacketType::UniversalMidiPacket96)
        {
            WINRT_ASSERT(false); // this will cause a break in debug builds

            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Incorrect UMP Packet type for Ump96", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiMessage96, midi2::MidiMessage96>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiPacketType::UniversalMidiPacket96) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillMessage128(
        midi2::MidiMessage128 const& ump)
    {
        if (ump == nullptr)
        {
            WINRT_ASSERT(false);    // this will cause a break in debug builds

            LOG_IF_FAILED(E_INVALIDARG);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"UMP parameter is null", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        if (PacketType() != MidiPacketType::UniversalMidiPacket128)
        {
            WINRT_ASSERT(false); // this will cause a break in debug builds

            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Incorrect UMP Packet type for Ump128", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiMessage128, midi2::MidiMessage128>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // TODO: Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiPacketType::UniversalMidiPacket128) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::FillWordArray(
        uint32_t const startIndex,
        winrt::array_view<uint32_t> words
        )
    {
        uint8_t messageWordCount = GetValidMessageWordCount();

        // make sure there's enough room in the array to hold the data we have
        if (words.size() - startIndex < messageWordCount)
            return 0;

        // copy over the words

        uint32_t* umpData = (uint32_t*)&m_data;

        for (int i = 0; i < messageWordCount; i++)
        {
            words[i + startIndex] = umpData[i];
        }

        return messageWordCount;
    }

    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::AppendWordsToList(
        collections::IVector<uint32_t> wordList
        )
    {
        uint8_t messageWordCount = GetValidMessageWordCount();

        // copy over the words

        uint32_t* umpData = (uint32_t*)&m_data;

        for (int i = 0; i < messageWordCount; i++)
        {
            wordList.Append(umpData[i]);
        }

        return messageWordCount;
    }


    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::FillByteArray(
        uint32_t const startIndex,
        winrt::array_view<uint8_t> bytes
        )
    {
        uint8_t messageByteCount = GetValidMessageWordCount() * sizeof(uint32_t);

        // make sure there's enough room in the array to hold the data we have
        if (bytes.size() - startIndex < messageByteCount)
            return 0;

        // copy over the bytes

        uint8_t* umpData = (uint8_t*)&m_data;

        for (int i = 0; i < messageByteCount; i++)
        {
            bytes[i + startIndex] = umpData[i];
        }

        return messageByteCount;
    }



    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::FillBuffer(
        uint32_t const byteOffset,
        foundation::IMemoryBuffer const& buffer
        )
    {
        try
        {
            auto ref = buffer.CreateReference();
            auto interop = ref.as<IMemoryBufferByteAccess>();

            uint8_t* value{};
            uint32_t valueSize{};

            // get a pointer to the buffer
            winrt::check_hresult(interop->GetBuffer(&value, &valueSize));

            uint8_t numWords = GetValidMessageWordCount();
            uint8_t numBytes = numWords * sizeof(uint32_t);

            if (byteOffset + numBytes > valueSize)
            {
                // no room
                return 0;
            }
            else
            {
                uint32_t* bufferWordPointer = reinterpret_cast<uint32_t*>(value + byteOffset);

                memcpy(bufferWordPointer, &m_data, numBytes);

                return numBytes;
            }
        }
        catch (winrt::hresult_error ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception filling buffer", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return 0;
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
                TraceLoggingWideString(L"hresult exception filling buffer", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return 0;
        }
    }


}
