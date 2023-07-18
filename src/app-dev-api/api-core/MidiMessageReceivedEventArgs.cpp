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
    MidiMessageReceivedEventArgs::MidiMessageReceivedEventArgs(PVOID data, UINT sizeInBytes, LONGLONG timestamp)
    {
        _timestamp = timestamp;

        if (sizeInBytes <= (uint32_t)(MidiUmpPacketType::Ump128) * sizeof(uint32_t))
        {
            memcpy(&_data, data, sizeInBytes);
        }
    }


    winrt::Windows::Devices::Midi2::MidiUmpPacketType MidiMessageReceivedEventArgs::UmpType()
    {
        if (_data.Word0 != 0)
        {
            return (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromMessageType(_data.Word0));
        }
        else
        {
            return MidiUmpPacketType::UnknownOrInvalid;

        }
    }

    winrt::Windows::Devices::Midi2::IMidiUmp MidiMessageReceivedEventArgs::GetUmp()
    {
        auto ty = (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromMessageType(_data.Word0));

        if (ty == MidiUmpPacketType::UnknownOrInvalid)
        {
            throw hresult_error();
        }


        if (ty == MidiUmpPacketType::Ump32)
        {
            return winrt::make_self<implementation::MidiUmp32>(_timestamp, _data.Word0).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else if (ty == MidiUmpPacketType::Ump64)
        {
            return winrt::make_self<implementation::MidiUmp64>(_timestamp, _data.Word0, _data.Word1).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else if (ty == MidiUmpPacketType::Ump96)
        {
            return winrt::make_self<implementation::MidiUmp96>(_timestamp, _data.Word0, _data.Word1, _data.Word2).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
        else if (ty == MidiUmpPacketType::Ump128)
        {
            return winrt::make_self<implementation::MidiUmp128>(_timestamp, _data.Word0, _data.Word1, _data.Word2, _data.Word3).as<winrt::Windows::Devices::Midi2::IMidiUmp>();
        }
    }

    bool MidiMessageReceivedEventArgs::FillWords(uint32_t& word0, uint32_t& word1, uint32_t& word2, uint32_t& word3)
    {
        auto ty = (MidiUmpPacketType)(internal::GetUmpLengthInMidiWordsFromMessageType(_data.Word0));

        if (ty == MidiUmpPacketType::UnknownOrInvalid)
        {
            return false;
        }
        else
        {
            word0 = _data.Word0;
        }
        
        if (ty == MidiUmpPacketType::Ump64 || ty == MidiUmpPacketType::Ump96 || ty == MidiUmpPacketType::Ump128)
        {
            word1 = _data.Word1;
        }

        if (ty == MidiUmpPacketType::Ump96 || ty == MidiUmpPacketType::Ump128)
        {
            word2 = _data.Word2;
        }

        if (ty == MidiUmpPacketType::Ump128)
        {
            word3 = _data.Word3;
        }

        return true;

    }

    bool MidiMessageReceivedEventArgs::FillUmp32(winrt::Windows::Devices::Midi2::MidiUmp32 const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillUmp64(winrt::Windows::Devices::Midi2::MidiUmp64 const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillUmp96(winrt::Windows::Devices::Midi2::MidiUmp96 const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillUmp128(winrt::Windows::Devices::Midi2::MidiUmp128 const& ump)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillWordArray(array_view<uint32_t> words)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillByteArray(array_view<uint8_t> bytes)
    {
        throw hresult_not_implemented();
    }

    bool MidiMessageReceivedEventArgs::FillBuffer(winrt::Windows::Foundation::IMemoryBuffer const& buffer, uint32_t byteOffset)
    {
        throw hresult_not_implemented();
    }
}
