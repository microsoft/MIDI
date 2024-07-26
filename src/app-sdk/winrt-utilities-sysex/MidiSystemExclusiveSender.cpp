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

#define DEFAULT_MESSAGE_SPACING_MILLISECONDS 2

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::SysEx::implementation
{
    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, uint32_t> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination, 
        streams::IDataReader dataSource, 
        sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
        sysex::MidiSystemExclusiveDataFormat dataFormat,
        uint8_t messageSpacingMilliseconds,
        midi2::MidiGroup newGroup) noexcept
    {
        UNREFERENCED_PARAMETER(destination);
        UNREFERENCED_PARAMETER(dataSource);
        UNREFERENCED_PARAMETER(sourceReaderFormat);
        UNREFERENCED_PARAMETER(dataFormat);
        UNREFERENCED_PARAMETER(messageSpacingMilliseconds);

        auto progress{ co_await winrt::get_progress_token() };

        uint32_t transferCount{ 0 };

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

        // if dataFormat is InferFromData, try to figure out the data format (sysex7, 8, byte, words, etc.)


        // do the transfer here

        progress(transferCount);



        co_return true;

    }


    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, uint32_t> MidiSystemExclusiveSender::SendDataAsync(
        midi2::MidiEndpointConnection destination,
        streams::IDataReader dataSource,
        sysex::MidiSystemExclusiveDataReaderFormat sourceReaderFormat,
        sysex::MidiSystemExclusiveDataFormat dataFormat) noexcept
    {
        return SendDataAsync(
            destination,
            dataSource,
            sourceReaderFormat,
            dataFormat,
            DEFAULT_MESSAGE_SPACING_MILLISECONDS,
            nullptr
        );
    }

    _Use_decl_annotations_
    foundation::IAsyncOperationWithProgress<bool, uint32_t> MidiSystemExclusiveSender::SendDataAsync(
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
