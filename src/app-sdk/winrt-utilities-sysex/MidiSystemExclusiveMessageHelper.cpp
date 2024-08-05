// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveMessageHelper.h"
#include "MidiSystemExclusiveMessageHelper.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation
{



    _Use_decl_annotations_
    uint8_t MidiSystemExclusiveMessageHelper::GetDataByteCountFromSystemExclusive7MessageFirstWord(uint32_t word0) noexcept
    {
        return MIDIWORDNIBBLE4(word0);
    }

    _Use_decl_annotations_
    uint8_t MidiSystemExclusiveMessageHelper::AppendDataBytesFromSingleSystemExclusive7Message(
        uint32_t const word0,
        uint32_t const word1,
        collections::IVector<uint8_t> dataBytesToAppendTo) noexcept
    {
        uint8_t messageByteCount = GetDataByteCountFromSystemExclusive7MessageFirstWord(word0);
        uint32_t currentWord = word0;
        uint8_t shift = 8;

        for (uint8_t i = 0; i < messageByteCount; i++)
        {
            dataBytesToAppendTo.Append((uint8_t)(currentWord >> shift & 0xFF)); // we don't & 0x7F in case the data is actually bad

            if (shift == 0)
            {
                currentWord = word1;
                shift = 24;
            }
            else
            {
                shift -= 8;
            }
        }

        return messageByteCount;
    }

    _Use_decl_annotations_
    uint8_t MidiSystemExclusiveMessageHelper::AppendDataBytesFromSingleSystemExclusive7Message(
        midi2::MidiMessage64 const& message,
        collections::IVector<uint8_t> dataBytesToAppendTo) noexcept
    {
        return AppendDataBytesFromSingleSystemExclusive7Message(message.Word0(), message.Word1(), dataBytesToAppendTo);
    }


    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiSystemExclusiveMessageHelper::GetDataBytesFromMultipleSystemExclusive7Messages(
        collections::IIterable<midi2::MidiMessage64> const& messages) noexcept
    {
        auto result = winrt::single_threaded_vector<uint8_t>();

        for (auto const& message : messages)
        {
            AppendDataBytesFromSingleSystemExclusive7Message(message, result);
        }

        return result;
    }

    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiSystemExclusiveMessageHelper::GetDataBytesFromSingleSystemExclusive7Message(
        uint32_t const word0,
        uint32_t const word1
    ) noexcept
    {
        auto result = winrt::single_threaded_vector<uint8_t>();

        AppendDataBytesFromSingleSystemExclusive7Message(word0, word1, result);

        return result;
    }

    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiSystemExclusiveMessageHelper::GetDataBytesFromSingleSystemExclusive7Message(
        midi2::MidiMessage64 const& message) noexcept
    {
        auto result = winrt::single_threaded_vector<uint8_t>();

        AppendDataBytesFromSingleSystemExclusive7Message(message.Word0(), message.Word1(), result);

        return result;
    }





    _Use_decl_annotations_
    bool MidiSystemExclusiveMessageHelper::MessageIsSystemExclusive7Message(uint32_t word0) noexcept
    {
        return internal::GetUmpMessageTypeFromFirstWord(word0) == MIDI_UMP_MESSAGE_TYPE_DATA_MESSAGE_64 &&
            internal::GetStatusFromDataMessage64FirstWord(word0) <= 0x03;   // 0x0, 0x1, 0x2, 0x3 are SysEx7
    }
}
