// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiStandardFileWriter.h"
#include "Utilities.Files.MidiStandardFileWriter.g.cpp"

#include "MidiFileWriteOptions.h"
#include "MidiFileWriteResult.h"
#include "MidiSequence.h"

#include "midi_file_smf_writer.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    namespace
    {
        Files::MidiFileWriteStatus TranslateStatus(_In_ ::midifile::WriteStatus const status) noexcept
        {
            switch (status)
            {
            case ::midifile::WriteStatus::Success:          return Files::MidiFileWriteStatus::Success;
            case ::midifile::WriteStatus::NothingToWrite:   return Files::MidiFileWriteStatus::NothingToWrite;
            case ::midifile::WriteStatus::TooMuchData:      return Files::MidiFileWriteStatus::TooLarge;
            case ::midifile::WriteStatus::AccessDenied:     return Files::MidiFileWriteStatus::AccessDenied;
            case ::midifile::WriteStatus::OutOfMemory:      return Files::MidiFileWriteStatus::OutOfMemory;
            default:                                        return Files::MidiFileWriteStatus::WriteError;
            }
        }

        Files::MidiFileWriteResult MakeResult(
            _In_ Files::MidiFileWriteStatus const status,
            _In_ uint32_t const trackCount,
            _In_ uint64_t const byteCount,
            _In_ uint32_t const skippedEventCount)
        {
            auto result = winrt::make_self<implementation::MidiFileWriteResult>();

            result->InternalInitialize(status, trackCount, byteCount, skippedEventCount);

            return *result;
        }

        Files::MidiFileWriteResult MakeResult(_In_ ::midifile::WriteResult const& written)
        {
            return MakeResult(
                TranslateStatus(written.Status),
                written.TrackCount,
                written.ByteCount,
                written.SkippedEventCount);
        }

        // The whole file is built in memory before any of it is written. A Standard MIDI File
        // declares each track's length in front of the track, so the length is not known until
        // the track has been laid out, and a partially written file is worse than none at all.
        foundation::IAsyncOperation<Files::MidiFileWriteResult> WriteStreamAsync(
            streams::IRandomAccessStream stream,
            std::shared_ptr<::midifile::MidiSequence const> sequence,
            ::midifile::WriteOptions options)
        {
            co_await winrt::resume_background();

            if (stream == nullptr || sequence == nullptr)
            {
                co_return MakeResult(Files::MidiFileWriteStatus::WriteError, 0, 0, 0);
            }

            std::vector<uint8_t> bytes{};

            auto const written = ::midifile::WriteStandardMidiFile(*sequence, bytes, options);

            if (!written.Succeeded())
            {
                co_return MakeResult(written);
            }

            try
            {
                streams::DataWriter writer{ stream.GetOutputStreamAt(0) };

                writer.WriteBytes(winrt::array_view<uint8_t const>{ bytes });

                auto const stored = co_await writer.StoreAsync();

                co_await writer.FlushAsync();

                writer.DetachStream();

                if (stored != bytes.size())
                {
                    co_return MakeResult(Files::MidiFileWriteStatus::WriteError, 0, stored, written.SkippedEventCount);
                }

                // A stream that already held a longer file would otherwise keep its tail.
                stream.Size(static_cast<uint64_t>(bytes.size()));
            }
            catch (...)
            {
                LOG_IF_FAILED(E_FAIL);

                co_return MakeResult(Files::MidiFileWriteStatus::WriteError, 0, 0, written.SkippedEventCount);
            }

            co_return MakeResult(written);
        }

        std::shared_ptr<::midifile::MidiSequence const> NativeSequenceOf(
            _In_ Sequencing::MidiSequence const& sequence) noexcept
        {
            if (sequence == nullptr)
            {
                return nullptr;
            }

            auto const* const self = winrt::get_self<Sequencing::implementation::MidiSequence>(sequence);

            return self == nullptr ? nullptr : self->InternalSequence();
        }

        ::midifile::WriteOptions NativeOptionsOf(_In_ Files::MidiFileWriteOptions const& options) noexcept
        {
            if (options == nullptr)
            {
                return ::midifile::WriteOptions{};
            }

            auto const* const self = winrt::get_self<implementation::MidiFileWriteOptions>(options);

            return self == nullptr ? ::midifile::WriteOptions{} : self->InternalOptions();
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileWriteResult> MidiStandardFileWriter::WriteAsync(
        streams::IRandomAccessStream stream,
        Sequencing::MidiSequence sequence)
    {
        return WriteAsync(stream, sequence, nullptr);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileWriteResult> MidiStandardFileWriter::WriteAsync(
        streams::IRandomAccessStream stream,
        Sequencing::MidiSequence sequence,
        Files::MidiFileWriteOptions options)
    {
        return WriteStreamAsync(stream, NativeSequenceOf(sequence), NativeOptionsOf(options));
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileWriteResult> MidiStandardFileWriter::WriteToFileAsync(
        winrt::Windows::Storage::StorageFile file,
        Sequencing::MidiSequence sequence)
    {
        return WriteToFileAsync(file, sequence, nullptr);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileWriteResult> MidiStandardFileWriter::WriteToFileAsync(
        winrt::Windows::Storage::StorageFile file,
        Sequencing::MidiSequence sequence,
        Files::MidiFileWriteOptions options)
    {
        if (file == nullptr)
        {
            co_return MakeResult(Files::MidiFileWriteStatus::WriteError, 0, 0, 0);
        }

        streams::IRandomAccessStream stream{ nullptr };

        try
        {
            stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            co_return MakeResult(Files::MidiFileWriteStatus::AccessDenied, 0, 0, 0);
        }

        co_return co_await WriteAsync(stream, sequence, options);
    }
}
