// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Files.MidiStandardFileReader.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    struct MidiStandardFileReader
    {
        MidiStandardFileReader() = default;

        static foundation::IAsyncOperation<Files::MidiFileReadResult> ReadAsync(
            _In_ streams::IRandomAccessStream stream);

        static foundation::IAsyncOperation<Files::MidiFileReadResult> ReadAsync(
            _In_ streams::IRandomAccessStream stream,
            _In_ Files::MidiFileReadOptions options);

        static foundation::IAsyncOperation<Files::MidiFileReadResult> ReadFromFileAsync(
            _In_ winrt::Windows::Storage::StorageFile file);

        static foundation::IAsyncOperation<Files::MidiFileReadResult> ReadFromFileAsync(
            _In_ winrt::Windows::Storage::StorageFile file,
            _In_ Files::MidiFileReadOptions options);
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Files::factory_implementation
{
    struct MidiStandardFileReader : MidiStandardFileReaderT<MidiStandardFileReader, implementation::MidiStandardFileReader>
    {
    };
}
