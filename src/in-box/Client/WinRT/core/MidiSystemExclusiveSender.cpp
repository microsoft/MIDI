// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSystemExclusiveSender.h"
#include "MidiSystemExclusiveSendProgress.h"
#include "MidiBytestreamToUmpMessageConverterState.h"
#include "Utilities.SysExTransfer.MidiSystemExclusiveSender.g.cpp"

#define DEFAULT_TRANSFER_SPACING_MILLISECONDS 100
#define DEFAULT_PREFERRED_SINGLE_TRANSFER_MESSAGE_COUNT 1000    // todo, this should be based on the max transfer as declared by the service/xproc

namespace winrt::Windows::Devices::Midi2::Utilities::SysExTransfer::implementation
{
    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, sysex::MidiSystemExclusiveSendProgress> MidiSystemExclusiveSender::SendBinarySysEx7ByteDataAsync(
        midi2::MidiEndpointConnection destinationConnection,
        midi2::MidiGroup destinationGroup,
        streams::IInputStream dataSource,
        uint32_t preferredSingleTransferMessageCount,
        uint16_t transferSpacingMilliseconds,
        msgs::MidiBytestreamToUmpMessageConverterState converterState
        )
    {
        co_await winrt::resume_background();

        auto progress{ co_await winrt::get_progress_token() };
        auto cancel{ co_await winrt::get_cancellation_token() };

        auto progressUpdate = winrt::make_self<MidiSystemExclusiveSendProgress>();

        if (converterState == nullptr)
        {
            // converterState is null
            throw winrt::hresult_error(E_INVALIDARG, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_MISSING_STATE_ARGUMENT));
        }

        if (dataSource == nullptr)
        {
            // dataSource is null
            throw winrt::hresult_error(E_INVALIDARG, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_MISSING_DATA_SOURCE_ARGUMENT));
        }

        if (destinationGroup == nullptr)
        {
            // destinationGroup is null
            throw winrt::hresult_error(E_INVALIDARG, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_MISSING_DESTINATION_GROUP_ARGUMENT));
        }

        if (destinationConnection == nullptr)
        {
            // connection is null
            throw winrt::hresult_error(E_INVALIDARG, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_MISSING_DESTINATION_CONNECTION_ARGUMENT));
        }

        if (!destinationConnection.IsOpen())
        {
            // connection is not open
            throw winrt::hresult_error(E_INVALIDARG, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_CLOSED_DESTINATION_CONNECTION_ARGUMENT));
        }

        // get the converter so we can continue across calls
        auto state = winrt::get_self<msgs::implementation::MidiBytestreamToUmpMessageConverterState>(converterState);
        std::shared_ptr<bytestreamToUMP> converter;
        converter = state->InternalGetConverter();


        streams::DataReader reader(dataSource);

        //reader.InputStreamOptions(streams::InputStreamOptions::ReadAhead);
          
        converter->defaultGroup = destinationGroup.Index();


        const uint32_t chunkSize = 512;

        // safe to block: this coroutine resumed on the thread pool above, so we are never on the
        // caller's apartment here
        auto bytesRead = reader.LoadAsync(chunkSize).get();

        //bool done = (bytesRead == 0);
        //bool lastPass = (bytesRead < chunkSize);
        bool success = false;

        //if (done)
        //{
        //    progressUpdate.LastErrorMessage = L"No data read from file.";
        //}


        progressUpdate->InternalInitialize(0,0);
        progress(*progressUpdate);

        uint32_t umpWords[2];       // Sysex7 is a 64 bit packet
        uint8_t wordIndex{ 0 };

        // data is binary sysex 7 bytestream format
        while (!cancel() && bytesRead > 0 && reader.UnconsumedBufferLength() > 0)
        {
            byte b = reader.ReadByte();
            converter->bytestreamParse(b);

            // todo: we may want to do this in chunks
            progressUpdate->InternalIncrementCountBytesRead();

            while (converter->availableUMP())
            {
                umpWords[wordIndex] = converter->readUMP();

                // Check for and handle non-sysex. Check to make sure it's a type 3. If it's not a type 3, then bail
                if (static_cast<midi2::MidiMessageType>(internal::GetUmpMessageTypeFromFirstWord(umpWords[0])) != midi2::MidiMessageType::DataMessage64)
                {
                    throw winrt::hresult_error(E_FAIL, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_UNEXPECTED_MESSAGE_IN_DATA));
                }

                // Type 3 messages are 64 bit. If we have a complete 64 bit message, then send it
                if (wordIndex == 1)
                {
                    if (midi2::MidiEndpointConnection::SendMessageSucceeded(
                        destinationConnection.SendSingleMessageWordArray(0, 0, _countof(umpWords), umpWords)))
                    {
                        progressUpdate->InternalIncrementCountMessagesSent();

                        wordIndex = 0;
                        umpWords[0] = 0;
                        umpWords[1] = 0;

                        if (transferSpacingMilliseconds != 0 && preferredSingleTransferMessageCount != 0 &&
                            (progressUpdate->CountMessagesSent() % preferredSingleTransferMessageCount == 0))
                        {
                            co_await winrt::resume_after(std::chrono::milliseconds(transferSpacingMilliseconds));
                        }
                    }
                    else
                    {
                        throw winrt::hresult_error(E_FAIL, internal::ResourceGetHString(IDS_SYSEX_TRANSFER_ERROR_SEND_MESSAGE_FAILED));
                    }
                }
                else
                {
                    wordIndex++;
                }

                progress(*progressUpdate);
            }

            if (reader.UnconsumedBufferLength() == 0)
            {
                bytesRead = co_await reader.LoadAsync(chunkSize);
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
        //progress(progressUpdate);
        co_return success;

    }



}
