// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveSender.h"
#include "MidiSystemExclusiveSender.g.cpp"

//#include <libmidi2/bytestreamToUMP.h>
#include "bytestreamToUMP.h"

#define DEFAULT_MESSAGE_SPACING_MILLISECONDS 2

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation
{
    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination, 
        streams::IDataReader dataSource, 
        sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
        sysex::MidiSystemExclusiveDataFormat sysexDataFormat,
        uint8_t messageSpacingMilliseconds,
        midi2::MidiGroup newGroup) noexcept
    {
        auto progress{ co_await winrt::get_progress_token() };
        auto cancel{ co_await winrt::get_cancellation_token() };

        sysex::MidiSystemExclusiveSendProgress progressUpdate{};

        bytestreamToUMP BS2UMP;

        if (destination == nullptr)
        {
            // connection is null
            co_return false;
        }

        if (!destination.IsOpen())
        {
            // connection is not open
            co_return false;
        }

        co_await winrt::resume_background();

        // if sourceReaderFormat is InferFromData, try to figure out the reader format (binary, text)

        if (sourceReaderFormat == sysex::MidiSystemExclusiveDataReaderFormat::Binary && sysexDataFormat == sysex::MidiSystemExclusiveDataFormat::ByteFormatSystemExclusive7)
        {
            // data is binary sysex 7 bytestream format
            while (dataSource.UnconsumedBufferLength() > 0 && !cancel())
            {
                byte b = dataSource.ReadByte();

                progressUpdate.BytesRead++;

                BS2UMP.bytestreamParse(b);

                // retrieve the UMP(s) from the parser
                // and sent it on
                while (BS2UMP.availableUMP())
                {
                    uint32_t umpWords[4];
                    uint8_t wordCount;

                    for (wordCount = 0; wordCount < _countof(umpWords) && BS2UMP.availableUMP(); wordCount++)
                    {
                        umpWords[wordCount] = BS2UMP.readUMP();
                    }

                    if (wordCount > 0)
                    {
                        destination.SendSingleMessageWordArray(0, 0, wordCount, umpWords);

                        progressUpdate.MessagesSent++;

                        if (messageSpacingMilliseconds > 0)
                        {
                            Sleep(messageSpacingMilliseconds);
                        }
                    }
                }

                progress(progressUpdate);
            }

            if (!cancel()) co_return true;

        }
        else if (sourceReaderFormat == sysex::MidiSystemExclusiveDataReaderFormat::Text)
        {
            // data is text format
        }
        else if (sourceReaderFormat == sysex::MidiSystemExclusiveDataReaderFormat::InferFromData)
        {
            // read the 
        }
        else
        {
            // invalid source reader format
            co_return false;
        }



        // return false until this is fully implemented
        co_return false;

    }


    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination,
        streams::IDataReader dataSource,
        sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
        sysex::MidiSystemExclusiveDataFormat sysexDataFormat) noexcept
    {
        return SendDataAsync(
            destination,
            dataSource,
            sourceReaderFormat,
            sysexDataFormat,
            DEFAULT_MESSAGE_SPACING_MILLISECONDS,
            nullptr
        );
    }


    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination,
        streams::IDataReader dataSource) noexcept
    {
        return SendDataAsync(
            destination,
            dataSource,
            sysex::MidiSystemExclusiveDataReaderFormat::InferFromData,
            sysex::MidiSystemExclusiveDataFormat::InferFromData
        );
    }



}
