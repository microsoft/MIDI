// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveSender.h"
#include "Utilities.SysExTransfer.MidiSystemExclusiveSender.g.cpp"

#include <bytestreamToUMP.h>

#define DEFAULT_MESSAGE_SPACING_MILLISECONDS 2

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination, 
        streams::IInputStream dataSource, 
        sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
        sysex::MidiSystemExclusiveDataFormat sysexDataFormat,
        uint16_t messageSpacingMilliseconds,
        bool changeGroup,
        midi2::MidiGroup newGroup) noexcept
    {
        co_await winrt::resume_background();

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



        // binary format MIDI 1.0 Data Format SysEx 7
        if (sourceReaderFormat == sysex::MidiSystemExclusiveDataReaderFormat::Binary && 
            sysexDataFormat == sysex::MidiSystemExclusiveDataFormat::ByteFormatSystemExclusive7)
        {
            streams::DataReader reader(dataSource);

            //reader.InputStreamOptions(streams::InputStreamOptions::ReadAhead);
          
            if (changeGroup && newGroup != nullptr)
            {
                BS2UMP.defaultGroup = newGroup.Index();
            }
            else
            {
                // TODO: for byte data, we do need a group. What we should do is 
                // pull from the group terminal blocks from the target device and
                // just grab the first valid midi out group index.

                BS2UMP.defaultGroup = 0;
            }

            const uint32_t chunkSize = 4096;
            auto bytesRead = co_await reader.LoadAsync(chunkSize);

            bool done = (bytesRead == 0);
            bool lastPass = (bytesRead < chunkSize);
            bool success = false;

            if (done)
            {
                progressUpdate.LastErrorMessage = L"No data read from file.";
            }

            progressUpdate.BytesRead = 0;
            progressUpdate.MessagesSent = 0;
            progress(progressUpdate);

            // data is binary sysex 7 bytestream format
            while (!done && !cancel())
            {
                while (reader.UnconsumedBufferLength() > 0)
                {
                    byte b = reader.ReadByte();
                    progressUpdate.BytesRead += 1;

                    BS2UMP.bytestreamParse(b);
                }

                progress(progressUpdate);

                // retrieve the UMP(s) from the parser
                // and sent it on
                while (BS2UMP.availableUMP())
                {
                    uint32_t umpWords[2];       // Sysex7 is a 64 bit packet
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

                if (!lastPass)
                {
                    auto bytesReadThisPass = co_await reader.LoadAsync(chunkSize);

                    if (bytesReadThisPass < chunkSize)
                    {
                        lastPass = true;
                    }
                }
                else
                {
                    // drained the end of the data
                    done = true;

                    success = true;
                }
            }

            co_return success;
        }
        else if (sourceReaderFormat == sysex::MidiSystemExclusiveDataReaderFormat::Text &&
            sysexDataFormat == sysex::MidiSystemExclusiveDataFormat::ByteFormatSystemExclusive7)
        {
            // data is text format MIDI 1.0 Data Format SysEx 7

        }

        // todo: other formats


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
        streams::IInputStream dataSource,
        sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
        sysex::MidiSystemExclusiveDataFormat sysexDataFormat) noexcept
    {
        return SendDataAsync(
            destination,
            dataSource,
            sourceReaderFormat,
            sysexDataFormat,
            DEFAULT_MESSAGE_SPACING_MILLISECONDS,
            false,
            nullptr
        );
    }


    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination,
        streams::IInputStream dataSource) noexcept
    {
        return SendDataAsync(
            destination,
            dataSource,
            sysex::MidiSystemExclusiveDataReaderFormat::InferFromData,
            sysex::MidiSystemExclusiveDataFormat::InferFromData
        );
    }



}
