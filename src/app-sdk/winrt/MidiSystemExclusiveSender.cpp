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

            const uint32_t chunkSize = 512;
            auto bytesRead = reader.LoadAsync(chunkSize).get();

            //bool done = (bytesRead == 0);
            //bool lastPass = (bytesRead < chunkSize);
            bool success = false;

            //if (done)
            //{
            //    progressUpdate.LastErrorMessage = L"No data read from file.";
            //}

            progressUpdate.BytesRead = 0;
            progressUpdate.MessagesSent = 0;
            progress(progressUpdate);

            uint32_t umpWords[2];       // Sysex7 is a 64 bit packet
            uint8_t wordIndex{ 0 };

            // data is binary sysex 7 bytestream format
            while (!cancel() && bytesRead > 0 && reader.UnconsumedBufferLength() > 0)
            {
                byte b = reader.ReadByte();
                BS2UMP.bytestreamParse(b);

                // todo: we may want to do this in chunks
                progressUpdate.BytesRead += 1;
                //progress(progressUpdate);

                while (BS2UMP.availableUMP())
                {
                    umpWords[wordIndex] = BS2UMP.readUMP();

                    // Check for and handle non-sysex. Check to make sure it's a type 3. If it's not a type 3, then bail
                    if (static_cast<MidiMessageType>(internal::GetUmpMessageTypeFromFirstWord(umpWords[0])) != MidiMessageType::DataMessage64)
                    {
                        progressUpdate.LastErrorMessage = L"Unexpected message found in source. Source should contain only System Exclusive messages and data.";
                        progress(progressUpdate);
                        co_return false;
                    }

                    // Type 3 messages are 64 bit. If we have a complete 64 bit message, then send it
                    if (wordIndex == 1)
                    {
                        if (MidiEndpointConnection::SendMessageSucceeded(destination.SendSingleMessageWordArray(0, 0, _countof(umpWords), umpWords)))
                        {
                            progressUpdate.MessagesSent++;

                            wordIndex = 0;
                            umpWords[0] = 0;
                            umpWords[1] = 0;

                            progress(progressUpdate);

                            if (messageSpacingMilliseconds > 0)
                            {
                                Sleep(messageSpacingMilliseconds);
                            }
                        }
                        else
                        {
                            // failed

                            progressUpdate.LastErrorMessage = L"Failed to send System Exclusive message.";
                            progress(progressUpdate);
                            co_return false;
                        }
                    }
                    else
                    {
                        wordIndex++;
                    }
                }

                if (reader.UnconsumedBufferLength() == 0)
                {
                    bytesRead = reader.LoadAsync(chunkSize).get();
                }


                

                //if (!lastPass)
                //{
                //    auto bytesReadThisPass = co_await reader.LoadAsync(chunkSize);

                //    if (bytesReadThisPass < chunkSize)
                //    {
                //        lastPass = true;
                //    }
                //}
                //else
                //{
                //    // drained the end of the data
                //    done = true;

                //    success = true;
                //}
            }

            success = true;
            progress(progressUpdate);
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
