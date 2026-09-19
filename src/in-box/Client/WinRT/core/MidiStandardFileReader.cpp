// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiStandardFileReader.h"
#include "Utilities.Files.MidiStandardFileReader.g.cpp"

#include "MidiFileReadOptions.h"
#include "MidiFileReadResult.h"
#include "MidiSequence.h"

#include "midi_file_smf_reader.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Files::implementation
{
    namespace
    {
        Files::MidiFileReadStatus TranslateStatus(_In_ ::midifile::ReadStatus const status) noexcept
        {
            switch (status)
            {
            case ::midifile::ReadStatus::Success:            return Files::MidiFileReadStatus::Success;
            case ::midifile::ReadStatus::NotAMidiFile:       return Files::MidiFileReadStatus::NotAMidiFile;
            case ::midifile::ReadStatus::CorruptHeader:      return Files::MidiFileReadStatus::CorruptData;
            case ::midifile::ReadStatus::NoPlayableData:     return Files::MidiFileReadStatus::NoPlayableData;
            case ::midifile::ReadStatus::FileTooLarge:       return Files::MidiFileReadStatus::TooLarge;
            case ::midifile::ReadStatus::TooMuchData:        return Files::MidiFileReadStatus::TooLarge;
            default:                                         return Files::MidiFileReadStatus::ReadError;
            }
        }

        Files::MidiFileReadResult MakeResult(
            _In_ Files::MidiFileReadStatus const status,
            _In_ bool const truncated,
            _In_ uint32_t const declaredTracks,
            _In_ uint32_t const readTracks,
            _In_ Sequencing::MidiSequence const& sequence)
        {
            auto result = winrt::make_self<implementation::MidiFileReadResult>();

            result->InternalInitialize(status, truncated, declaredTracks, readTracks, sequence);

            return *result;
        }

        Files::MidiFileReadResult ReadFromBytes(
            _In_ std::vector<uint8_t> const& bytes,
            _In_ ::midifile::ReadLimits const& limits,
            _In_ bool const failOnUnreadableData)
        {
            auto sequence = std::make_shared<::midifile::MidiSequence>();

            auto const read = ::midifile::ParseStandardMidiFile(
                std::span<uint8_t const>{ bytes.data(), bytes.size() }, *sequence, limits);

            auto status = TranslateStatus(read.Status);

            // A file that stopped making sense partway is still worth hearing unless the caller
            // said otherwise.
            if (status == Files::MidiFileReadStatus::Success && read.Truncated && failOnUnreadableData)
            {
                status = Files::MidiFileReadStatus::CorruptData;
            }

            if (status != Files::MidiFileReadStatus::Success)
            {
                return MakeResult(status, read.Truncated, read.TracksDeclared, read.TracksRead, nullptr);
            }

            auto projected = winrt::make_self<Sequencing::implementation::MidiSequence>();

            projected->InternalInitialize(sequence);

            return MakeResult(status, read.Truncated, read.TracksDeclared, read.TracksRead, *projected);
        }

        // Reading the whole file before parsing is deliberate. The parser needs to seek around a
        // file whose chunk lengths are not to be trusted, and the ceiling on what may be read is
        // what keeps that safe.
        foundation::IAsyncOperation<Files::MidiFileReadResult> ReadStreamAsync(
            streams::IRandomAccessStream stream,
            ::midifile::ReadLimits limits,
            bool failOnUnreadableData)
        {
            co_await winrt::resume_background();

            if (stream == nullptr)
            {
                co_return MakeResult(Files::MidiFileReadStatus::ReadError, false, 0, 0, nullptr);
            }

            auto const size = stream.Size();

            if (size == 0)
            {
                co_return MakeResult(Files::MidiFileReadStatus::NotAMidiFile, false, 0, 0, nullptr);
            }

            if (size > limits.MaximumFileBytes)
            {
                co_return MakeResult(Files::MidiFileReadStatus::TooLarge, false, 0, 0, nullptr);
            }

            std::vector<uint8_t> bytes{};

            try
            {
                bytes.resize(static_cast<size_t>(size));
            }
            catch (...)
            {
                co_return MakeResult(Files::MidiFileReadStatus::TooLarge, false, 0, 0, nullptr);
            }

            try
            {
                streams::DataReader reader{ stream.GetInputStreamAt(0) };

                auto const loaded = co_await reader.LoadAsync(static_cast<uint32_t>(size));

                if (loaded != size)
                {
                    co_return MakeResult(Files::MidiFileReadStatus::ReadError, false, 0, 0, nullptr);
                }

                reader.ReadBytes(winrt::array_view<uint8_t>{ bytes });
            }
            catch (...)
            {
                LOG_IF_FAILED(E_FAIL);

                co_return MakeResult(Files::MidiFileReadStatus::ReadError, false, 0, 0, nullptr);
            }

            co_return ReadFromBytes(bytes, limits, failOnUnreadableData);
        }
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileReadResult> MidiStandardFileReader::ReadAsync(
        streams::IRandomAccessStream stream)
    {
        return ReadStreamAsync(stream, ::midifile::ReadLimits{}, false);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileReadResult> MidiStandardFileReader::ReadAsync(
        streams::IRandomAccessStream stream,
        Files::MidiFileReadOptions options)
    {
        if (options == nullptr)
        {
            return ReadStreamAsync(stream, ::midifile::ReadLimits{}, false);
        }

        auto const* const self = winrt::get_self<implementation::MidiFileReadOptions>(options);

        return ReadStreamAsync(stream, self->InternalLimits(), self->FailOnUnreadableData());
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileReadResult> MidiStandardFileReader::ReadFromFileAsync(
        winrt::Windows::Storage::StorageFile file)
    {
        return ReadFromFileAsync(file, nullptr);
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<Files::MidiFileReadResult> MidiStandardFileReader::ReadFromFileAsync(
        winrt::Windows::Storage::StorageFile file,
        Files::MidiFileReadOptions options)
    {
        if (file == nullptr)
        {
            co_return MakeResult(Files::MidiFileReadStatus::ReadError, false, 0, 0, nullptr);
        }

        streams::IRandomAccessStream stream{ nullptr };

        try
        {
            stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);

            co_return MakeResult(Files::MidiFileReadStatus::ReadError, false, 0, 0, nullptr);
        }

        co_return co_await ReadAsync(stream, options);
    }
}
