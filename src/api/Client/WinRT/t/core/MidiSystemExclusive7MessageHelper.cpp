// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusive7MessageHelper.h"
#include "Utilities.Messages.MidiSystemExclusive7MessageHelper.g.cpp"


namespace winrt::Windows::Devices::Midi2::Utilities::Messages::implementation
{



    _Use_decl_annotations_
    uint8_t MidiSystemExclusive7MessageHelper::GetDataByteCountFromSystemExclusiveMessageFirstWord(uint32_t word0) noexcept
    {
        return MIDIWORDNIBBLE4(word0);
    }

    _Use_decl_annotations_
    uint8_t MidiSystemExclusive7MessageHelper::AppendDataBytesFromSingleSystemExclusiveMessage(
        uint32_t const word0,
        uint32_t const word1,
        collections::IVector<uint8_t> dataBytesToAppendTo) noexcept
    {
        try
        {
        uint8_t messageByteCount = GetDataByteCountFromSystemExclusiveMessageFirstWord(word0);
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error appending data bytes from single system exclusive message.");
            return 0;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception appending data bytes from single system exclusive message.");
            return 0;
        }
    }

    _Use_decl_annotations_
    uint8_t MidiSystemExclusive7MessageHelper::AppendDataBytesFromSingleSystemExclusiveMessage(
        midi2::MidiMessage64 const& message,
        collections::IVector<uint8_t> dataBytesToAppendTo) noexcept
    {
        try
        {
        return AppendDataBytesFromSingleSystemExclusiveMessage(message.Word0(), message.Word1(), dataBytesToAppendTo);
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error appending data bytes from single system exclusive message.");
            return 0;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception appending data bytes from single system exclusive message.");
            return 0;
        }
    }


    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiSystemExclusive7MessageHelper::GetDataBytesFromMultipleSystemExclusiveMessages(
        collections::IIterable<midi2::MidiMessage64> const& messages) noexcept
    {
        try
        {
        auto result = winrt::single_threaded_vector<uint8_t>();

        for (auto const& message : messages)
        {
            AppendDataBytesFromSingleSystemExclusiveMessage(message, result);
        }

        return result;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting data bytes from multiple system exclusive messages.");
            return winrt::single_threaded_vector<uint8_t>();
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting data bytes from multiple system exclusive messages.");
            return winrt::single_threaded_vector<uint8_t>();
        }
    }

    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(
        uint32_t const word0,
        uint32_t const word1
    ) noexcept
    {
        try
        {
        auto result = winrt::single_threaded_vector<uint8_t>();

        AppendDataBytesFromSingleSystemExclusiveMessage(word0, word1, result);

        return result;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting data bytes from single system exclusive message.");
            return winrt::single_threaded_vector<uint8_t>();
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting data bytes from single system exclusive message.");
            return winrt::single_threaded_vector<uint8_t>();
        }
    }

    _Use_decl_annotations_
    collections::IVector<uint8_t> MidiSystemExclusive7MessageHelper::GetDataBytesFromSingleSystemExclusiveMessage(
        midi2::MidiMessage64 const& message) noexcept
    {
        try
        {
        auto result = winrt::single_threaded_vector<uint8_t>();

        AppendDataBytesFromSingleSystemExclusiveMessage(message.Word0(), message.Word1(), result);

        return result;
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error getting data bytes from single system exclusive message.");
            return winrt::single_threaded_vector<uint8_t>();
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception getting data bytes from single system exclusive message.");
            return winrt::single_threaded_vector<uint8_t>();
        }
    }





    _Use_decl_annotations_
    bool MidiSystemExclusive7MessageHelper::MessageIsSystemExclusiveMessage(uint32_t word0) noexcept
    {
        return internal::GetUmpMessageTypeFromFirstWord(word0) == MIDI_UMP_MESSAGE_TYPE_DATA_MESSAGE_64 &&
            internal::GetStatusFromDataMessage64FirstWord(word0) <= 0x03;   // 0x0, 0x1, 0x2, 0x3 are SysEx7
    }
}
