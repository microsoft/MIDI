// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Files.MidiStandardFileWriter.g.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    struct MidiStandardFileWriter
    {
        MidiStandardFileWriter() = default;

        static foundation::IAsyncOperation<Files::MidiFileWriteResult> WriteAsync(
            _In_ streams::IRandomAccessStream stream,
            _In_ Sequencing::MidiSequence sequence);

        static foundation::IAsyncOperation<Files::MidiFileWriteResult> WriteAsync(
            _In_ streams::IRandomAccessStream stream,
            _In_ Sequencing::MidiSequence sequence,
            _In_ Files::MidiFileWriteOptions options);

        static foundation::IAsyncOperation<Files::MidiFileWriteResult> WriteToFileAsync(
            _In_ winrt::Windows::Storage::StorageFile file,
            _In_ Sequencing::MidiSequence sequence);

        static foundation::IAsyncOperation<Files::MidiFileWriteResult> WriteToFileAsync(
            _In_ winrt::Windows::Storage::StorageFile file,
            _In_ Sequencing::MidiSequence sequence,
            _In_ Files::MidiFileWriteOptions options);
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Files::factory_implementation
{
    struct MidiStandardFileWriter : MidiStandardFileWriterT<MidiStandardFileWriter, implementation::MidiStandardFileWriter>
    {
    };
}
