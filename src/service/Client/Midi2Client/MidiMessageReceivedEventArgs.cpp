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
    MidiMessageReceivedEventArgs::MidiMessageReceivedEventArgs(_In_ PVOID data, _In_ UINT sizeInBytes, _In_ LONGLONG timestamp)
    {
        m_timestamp = timestamp;

        if (sizeInBytes <= (uint32_t)(MidiUmpPacketType::Ump128) * sizeof(uint32_t))
        {
            memcpy(&m_data, data, sizeInBytes);
        }
    }


    winrt::Windows::Devices::Midi2::MidiUmpPacketType MidiMessageReceivedEventArgs::UmpType()
    {
        if (m_data.Word0 != 0)
        {
            return (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0));
        }
        else
        {
            return MidiUmpPacketType::UnknownOrInvalid;

        }
    }

    winrt::Windows::Devices::Midi2::IMidiUmp MidiMessageReceivedEventArgs::GetUmp()
    {
        auto ty = (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0));

        if (ty == MidiUmpPacketType::UnknownOrInvalid)
        {          
            internal::LogGeneralError(__FUNCTION__, L"Invalid UMP Packet type");

            throw hresult_error();
        }


        if (ty == MidiUmpPacketType::Ump32)
        {
            return winrt::make_self<implementation::MidiUmp32>(m_timestamp, m_data.Word0).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else if (ty == MidiUmpPacketType::Ump64)
        {
            return winrt::make_self<implementation::MidiUmp64>(m_timestamp, m_data.Word0, m_data.Word1).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else if (ty == MidiUmpPacketType::Ump96)
        {
            return winrt::make_self<implementation::MidiUmp96>(m_timestamp, m_data.Word0, m_data.Word1, m_data.Word2).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else if (ty == MidiUmpPacketType::Ump128)
        {
            return winrt::make_self<implementation::MidiUmp128>(m_timestamp, m_data.Word0, m_data.Word1, m_data.Word2, m_data.Word3).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else
        {
            // this should never happen

            internal::LogGeneralError(__FUNCTION__, L"Invalid UMP Packet type");

            return nullptr;
        }
    }

    bool MidiMessageReceivedEventArgs::FillWords(
        _Inout_ uint32_t& word0, 
        _Inout_ uint32_t& word1, 
        _Inout_ uint32_t& word2,
        _Inout_ uint32_t& word3)
    {
        auto ty = (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromFirstWord(m_data.Word0));

        if (ty == MidiUmpPacketType::UnknownOrInvalid)
        {
            internal::LogGeneralError(__FUNCTION__, L"Invalid UMP Packet type");

            return false;
        }
        else
        {
            word0 = m_data.Word0;
        }
        
        if (ty == MidiUmpPacketType::Ump64 || ty == MidiUmpPacketType::Ump96 || ty == MidiUmpPacketType::Ump128)
        {
            word1 = m_data.Word1;
        }

        if (ty == MidiUmpPacketType::Ump96 || ty == MidiUmpPacketType::Ump128)
        {
            word2 = m_data.Word2;
        }

        if (ty == MidiUmpPacketType::Ump128)
        {
            word3 = m_data.Word3;
        }

        return true;

    }

    bool MidiMessageReceivedEventArgs::FillUmp32(_In_ winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
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

        auto impUmp = winrt::get_self<implementation::MidiUmp32, winrt::Windows::Devices::Midi2::MidiUmp32>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump32) * sizeof(uint32_t));

        return true;
    }

    bool MidiMessageReceivedEventArgs::FillUmp64(_In_ winrt::Windows::Devices::Midi2::MidiUmp64 const& ump)
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

        auto impUmp = winrt::get_self<implementation::MidiUmp64, winrt::Windows::Devices::Midi2::MidiUmp64>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump64) * sizeof(uint32_t));

        return true;
    }

    bool MidiMessageReceivedEventArgs::FillUmp96(_In_ winrt::Windows::Devices::Midi2::MidiUmp96 const& ump)
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

        auto impUmp = winrt::get_self<implementation::MidiUmp96, winrt::Windows::Devices::Midi2::MidiUmp96>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump96) * sizeof(uint32_t));

        return true;
    }

    bool MidiMessageReceivedEventArgs::FillUmp128(_In_ winrt::Windows::Devices::Midi2::MidiUmp128 const& ump)
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

        auto impUmp = winrt::get_self<implementation::MidiUmp128, winrt::Windows::Devices::Midi2::MidiUmp128>(ump);
        WINRT_ASSERT(impUmp != nullptr);

        auto umpDestinationData = impUmp->GetInternalUmpDataPointer();
        WINRT_ASSERT(umpDestinationData != nullptr);

        // copy data over. TODO: need to error check this
        // Consider adding a write lock to UMP to guard this as well
        memcpy(umpDestinationData, &m_data, (uint32_t)(MidiUmpPacketType::Ump128) * sizeof(uint32_t));

        return true;
    }

    bool MidiMessageReceivedEventArgs::FillWordArray(
        _In_ array_view<uint32_t> /* words */, 
        _In_ uint32_t /*index*/, 
        _Out_ uint32_t& /*elementsWritten*/)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillByteArray(
        _In_ array_view<uint8_t> /* bytes */, 
        _In_ uint32_t /*index*/, 
        _Out_ uint32_t& /*elementsWritten*/)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillBuffer(
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& /* buffer */, 
        _In_ uint32_t /* byteOffset*/,
        _Out_ uint32_t& /* bytesWritten*/)
    {
        throw hresult_not_implemented();
    }

}
