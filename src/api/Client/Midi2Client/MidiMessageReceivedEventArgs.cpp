// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include "pch.h"
#include "MidiMessageReceivedEventArgs.h"
#include "MidiMessageReceivedEventArgs.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    MidiMessageReceivedEventArgs::MidiMessageReceivedEventArgs(PVOID data, UINT sizeInBytes, internal::MidiTimestamp timestamp)
    {
        // this is making the assumption that what comes through here is somehow a valid UMP. We'll happily
        // copy over all the data, but what the client will receive when it grabs the UMP will be based
        // on the message type parsed from word0

        m_timestamp = timestamp;

        if (sizeInBytes <= (uint32_t)(MidiUmpPacketType::Ump128) * sizeof(uint32_t) && 
            sizeInBytes > 0 && 
            data != nullptr)
        {
            memcpy(&m_data, data, sizeInBytes);
        }
    }

    _Use_decl_annotations_
    midi2::MidiUmpPacketType MidiMessageReceivedEventArgs::UmpType() const noexcept
    {
        // a type 0 ump with no data is just the noop, and is perfectly valid, so we always run this

        return (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0));
    }

    _Use_decl_annotations_
    midi2::MidiUmpMessageType MidiMessageReceivedEventArgs::UmpMessageType() const noexcept
    {
        return (MidiUmpMessageType)(internal::GetUmpMessageTypeFromFirstWord(m_data.Word0));
    }


    _Use_decl_annotations_
    midi2::IMidiUmp MidiMessageReceivedEventArgs::GetUmp()
    {
        auto wordCount = GetValidMessageWordCount();

        if (wordCount == 1) // ump32
        {
            return winrt::make_self<implementation::MidiUmp32>(m_timestamp, m_data.Word0).as<midi2::IMidiUmp>();
        }
        else if (wordCount == 2)    // ump64
        {
            return winrt::make_self<implementation::MidiUmp64>(m_timestamp, m_data.Word0, m_data.Word1).as<midi2::IMidiUmp>();
        }
        else if (wordCount == 3) // ump96
        {
            return winrt::make_self<implementation::MidiUmp96>(m_timestamp, m_data.Word0, m_data.Word1, m_data.Word2).as<midi2::IMidiUmp>();
        }
        else if (wordCount == 4) // ump128
        {
            return winrt::make_self<implementation::MidiUmp128>(m_timestamp, m_data.Word0, m_data.Word1, m_data.Word2, m_data.Word3).as<midi2::IMidiUmp>();
        }
        else
        {
            // this should never happen

            internal::LogGeneralError(__FUNCTION__, L"Invalid UMP Packet type");

            return nullptr;
        }
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
    bool MidiMessageReceivedEventArgs::FillUmp32(midi2::MidiUmp32 const& ump)
    {
        if (ump == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"UMP parameter is null");
            return false;
        }

        if (UmpType() != MidiUmpPacketType::Ump32)
        {
            internal::LogGeneralError(__FUNCTION__, L"Incorrect UMP Packet type for Ump32");
            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiUmp32, midi2::MidiUmp32>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump32) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillUmp64(midi2::MidiUmp64 const& ump)
    {
        if (ump == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"UMP parameter is null");
            return false;
        }

        if (UmpType() != MidiUmpPacketType::Ump64)
        {
            internal::LogGeneralError(__FUNCTION__, L"Incorrect UMP Packet type for Ump64");
            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiUmp64, midi2::MidiUmp64>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump64) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillUmp96(midi2::MidiUmp96 const& ump)
    {
        if (ump == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"UMP parameter is null");
            return false;
        }

        if (UmpType() != MidiUmpPacketType::Ump96)
        {
            internal::LogGeneralError(__FUNCTION__, L"Incorrect UMP Packet type for Ump96");
            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiUmp96, midi2::MidiUmp96>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump96) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    bool MidiMessageReceivedEventArgs::FillUmp128(midi2::MidiUmp128 const& ump)
    {
        if (ump == nullptr)
        {
            internal::LogGeneralError(__FUNCTION__, L"UMP parameter is null");
            return false;
        }

        if (UmpType() != MidiUmpPacketType::Ump128)
        {
            internal::LogGeneralError(__FUNCTION__, L"Incorrect UMP Packet type for Ump128");
            return false;
        }

        auto impUmp = winrt::get_self<implementation::MidiUmp128, midi2::MidiUmp128>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // TODO: Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump128) * sizeof(uint32_t));

        return true;
    }

    _Use_decl_annotations_
    uint8_t MidiMessageReceivedEventArgs::FillWordArray(
        winrt::array_view<uint32_t> words, 
        uint32_t const startIndex
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
    uint8_t MidiMessageReceivedEventArgs::FillByteArray(
        winrt::array_view<uint8_t> bytes, 
        uint32_t const startIndex
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
        foundation::IMemoryBuffer const& /* buffer */, 
        uint32_t const /* byteOffset*/
        )
    {
        throw hresult_not_implemented();
    }


}
