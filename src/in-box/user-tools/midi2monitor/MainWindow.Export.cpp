// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Export of the full capture, including messages that are filtered out of the display.
//
// Three shapes, because they are wanted for three different reasons. The text file is for
// reading. The comma separated file is for a script or a spreadsheet, so its column names are
// fixed English and its times are plain numbers. The Standard MIDI File is for playing the
// capture back, or loading it into a sequencer.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "MidiMessageDecoder.h"
#include "StringResources.h"
#include "TimestampFormatter.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace files = ::winrt::Windows::Devices::Midi2::Utilities::Files;
namespace sequencing = ::winrt::Windows::Devices::Midi2::Utilities::Sequencing;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        constexpr size_t ExportFlushThresholdCharacters = 1u << 16;

        // Ticks per quarter note for the Standard MIDI File, at a fixed 120 beats per minute, so
        // one tick is a little over half a millisecond. This is the division nearly every
        // sequencer uses natively. A capture being studied for timing wants the comma separated
        // export instead, which carries the real microsecond figures.
        constexpr uint16_t SmfTicksPerQuarterNote = 960;
        constexpr double SmfBeatsPerMinute = 120.0;
        constexpr double SmfMicrosecondsPerQuarterNote = 60000000.0 / SmfBeatsPerMinute;

        enum class ExportFormat
        {
            Text,
            CommaSeparated,
            StandardMidiFile
        };

        ExportFormat FormatFromFileType(winrt::hstring const& fileType) noexcept
        {
            std::wstring extension{ fileType };

            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](wchar_t value) noexcept { return static_cast<wchar_t>(::towlower(value)); });

            if (extension == L".csv")
            {
                return ExportFormat::CommaSeparated;
            }

            if (extension == L".mid" || extension == L".midi")
            {
                return ExportFormat::StandardMidiFile;
            }

            return ExportFormat::Text;
        }

        void AppendField(std::wstring& line, std::wstring_view value)
        {
            line.append(value);
            line.append(L"\t");
        }

        // RFC 4180: a field is quoted when it holds a comma, a quote or a line break, and an
        // embedded quote is doubled.
        void AppendCsvField(std::wstring& line, std::wstring_view value, bool last = false)
        {
            if (value.find_first_of(L",\"\r\n") != std::wstring_view::npos)
            {
                line.append(L"\"");

                for (auto const character : value)
                {
                    if (character == L'\"')
                    {
                        line.append(L"\"");
                    }

                    line.push_back(character);
                }

                line.append(L"\"");
            }
            else
            {
                line.append(value);
            }

            line.append(last ? L"\r\n" : L",");
        }

        std::wstring BuildTextHeader()
        {
            std::wstring line{};

            AppendField(line, res::GetString(L"ExportHeaderIndex"));
            AppendField(line, res::GetString(L"ExportHeaderTimestamp"));
            AppendField(line, res::GetString(L"ExportHeaderDelta"));
            AppendField(line, res::GetString(L"ExportHeaderGroup"));
            AppendField(line, res::GetString(L"ExportHeaderChannel"));
            AppendField(line, res::GetString(L"ExportHeaderData"));
            AppendField(line, res::GetString(L"ExportHeaderMessageName"));
            AppendField(line, res::GetString(L"ExportHeaderDecoded"));

            line.append(L"\r\n");

            return line;
        }

        // Deliberately NOT localized. A script reading this file finds its columns by name, and a
        // column called "Gruppe" on a German machine would break it.
        std::wstring BuildCommaSeparatedHeader()
        {
            return L"Index,TimestampTicks,OffsetMicroseconds,DeltaMicroseconds,Group,Channel,"
                   L"WordCount,Word0,Word1,Word2,Word3,MessageName,Decoded,Comment\r\n";
        }

        std::wstring CleanForSingleLine(std::wstring_view value)
        {
            std::wstring text{ value };

            std::replace(text.begin(), text.end(), L'\t', L' ');
            std::replace(text.begin(), text.end(), L'\r', L' ');
            std::replace(text.begin(), text.end(), L'\n', L' ');

            return text;
        }

        std::wstring FormatMicroseconds(uint64_t ticks)
        {
            auto const microseconds =
                (static_cast<double>(ticks) / native::TimestampFormatter::TimestampFrequency()) * 1000000.0;

            return std::format(L"{:.3f}", microseconds);
        }

        std::wstring BuildExportLine(
            native::MessageRecord const& record,
            native::TimestampDisplayFormat timestampFormat)
        {
            std::wstring line{};

            if (record.Kind == native::RecordKind::Notice)
            {
                line.append(L"# ");
                line.append(record.NoticeText);
                line.append(L"\r\n");

                return line;
            }

            AppendField(line, std::format(L"{}", record.MessageIndex));
            AppendField(line, native::TimestampFormatter::FormatTimestamp(record.Timestamp, timestampFormat).ToDisplayString());
            AppendField(line, native::TimestampFormatter::FormatDelta(record.DeltaTicks).ToDisplayString());
            AppendField(line, record.HasGroup ? std::format(L"{}", record.GroupNumber) : std::wstring{});
            AppendField(line, record.HasChannel ? std::format(L"{}", record.ChannelNumber) : std::wstring{});

            std::wstring words{};

            for (uint8_t i = 0; i < record.WordCount && i < 4; i++)
            {
                if (i > 0)
                {
                    words.append(L" ");
                }

                words.append(std::format(L"{:08X}", record.Words[i]));
            }

            AppendField(line, words);
            AppendField(line, native::GetMessageDisplayName(record.Words[0]));

            line.append(CleanForSingleLine(native::DecodeMessage(record).ToDisplayString()));

            if (!record.Comment.empty())
            {
                line.append(L"\t# ");
                line.append(CleanForSingleLine(record.Comment));
            }

            line.append(L"\r\n");

            return line;
        }

        std::wstring BuildCommaSeparatedLine(native::MessageRecord const& record)
        {
            std::wstring line{};

            // A notice is not a MIDI message. It keeps its text and reports no words, which is
            // what lets a reader skip it with a test on WordCount.
            if (record.Kind == native::RecordKind::Notice)
            {
                for (int field = 0; field < 12; field++)
                {
                    AppendCsvField(line, field == 6 ? L"0" : L"");
                }

                AppendCsvField(line, CleanForSingleLine(record.NoticeText));
                AppendCsvField(line, L"", true);

                return line;
            }

            AppendCsvField(line, std::format(L"{}", record.MessageIndex));
            AppendCsvField(line, std::format(L"{}", record.Timestamp));
            AppendCsvField(line, FormatMicroseconds(record.OffsetTicks));
            AppendCsvField(line, FormatMicroseconds(record.DeltaTicks));
            AppendCsvField(line, record.HasGroup ? std::format(L"{}", record.GroupNumber) : std::wstring{});
            AppendCsvField(line, record.HasChannel ? std::format(L"{}", record.ChannelNumber) : std::wstring{});
            AppendCsvField(line, std::format(L"{}", record.WordCount));

            for (uint8_t i = 0; i < 4; i++)
            {
                AppendCsvField(line, i < record.WordCount ? std::format(L"{:08X}", record.Words[i]) : std::wstring{});
            }

            AppendCsvField(line, std::wstring{ native::GetMessageDisplayName(record.Words[0]) });
            AppendCsvField(line, CleanForSingleLine(native::DecodeMessage(record).ToDisplayString()));
            AppendCsvField(line, CleanForSingleLine(record.Comment), true);

            return line;
        }

        bool WriteUtf8(HANDLE file, std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return true;
            }

            auto const required = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (required <= 0)
            {
                return false;
            }

            std::string buffer(static_cast<size_t>(required), '\0');

            if (::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                buffer.data(), required, nullptr, nullptr) <= 0)
            {
                return false;
            }

            DWORD written{ 0 };

            return ::WriteFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr) != FALSE;
        }

        // Runs on a background thread. Streams in chunks so a full buffer never has to be
        // materialized as one enormous string.
        HRESULT WriteTextExportFile(
            winrt::hstring const& path,
            std::vector<native::MessageRecord> const& records,
            native::TimestampDisplayFormat timestampFormat,
            ExportFormat format) noexcept
        {
            try
            {
                wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };

                RETURN_LAST_ERROR_IF(!file);

                static constexpr char utf8ByteOrderMark[]{ "\xEF\xBB\xBF" };
                DWORD written{ 0 };
                RETURN_LAST_ERROR_IF(!::WriteFile(file.get(), utf8ByteOrderMark, 3, &written, nullptr));

                auto const comma = format == ExportFormat::CommaSeparated;

                std::wstring buffer{ comma ? BuildCommaSeparatedHeader() : BuildTextHeader() };

                for (auto const& record : records)
                {
                    buffer.append(comma
                        ? BuildCommaSeparatedLine(record)
                        : BuildExportLine(record, timestampFormat));

                    if (buffer.size() >= ExportFlushThresholdCharacters)
                    {
                        RETURN_HR_IF(E_FAIL, !WriteUtf8(file.get(), buffer));
                        buffer.clear();
                    }
                }

                RETURN_HR_IF(E_FAIL, !WriteUtf8(file.get(), buffer));

                return S_OK;
            }
            CATCH_RETURN();
        }

        // Builds a sequence from the capture and hands it to the SDK's file writer, which is the
        // same path any other application would take.
        sequencing::MidiSequence BuildSequenceFromCapture(
            std::vector<native::MessageRecord> const& records,
            winrt::hstring const& endpointName)
        {
            sequencing::MidiSequenceBuilder builder{};

            builder.TicksPerQuarterNote(SmfTicksPerQuarterNote);
            builder.AddTempoChange(0, SmfBeatsPerMinute);

            // One track per group that actually appears, so which group a message arrived on is
            // still readable after the file is loaded into a sequencer. There is no other place
            // in a Standard MIDI File to keep it.
            std::array<uint16_t, 16> trackForGroup{};
            std::array<bool, 16> groupSeen{};

            for (auto const& record : records)
            {
                if (record.Kind == native::RecordKind::MidiMessage && record.HasGroup &&
                    record.GroupNumber >= 1 && record.GroupNumber <= 16)
                {
                    groupSeen[record.GroupNumber - 1] = true;
                }
            }

            std::wstring const baseName = endpointName.empty()
                ? std::wstring{ res::GetString(L"ExportSequenceDefaultTrackName") }
                : std::wstring{ endpointName };

            auto const ungroupedTrack = builder.AddTrack(winrt::hstring{ baseName });

            for (uint8_t group = 0; group < 16; group++)
            {
                if (!groupSeen[group])
                {
                    continue;
                }

                trackForGroup[group] = builder.AddTrack(
                    res::FormatString(L"ExportSequenceGroupTrackNameFormat", baseName, group + 1));
            }

            uint64_t origin = 0;
            bool haveOrigin = false;
            uint32_t previousTick = 0;

            for (auto const& record : records)
            {
                if (record.Kind != native::RecordKind::MidiMessage || record.WordCount == 0)
                {
                    continue;
                }

                if (!haveOrigin)
                {
                    origin = record.Timestamp;
                    haveOrigin = true;
                }

                auto const elapsed = record.Timestamp > origin ? record.Timestamp - origin : 0ull;

                auto const microseconds =
                    (static_cast<double>(elapsed) / native::TimestampFormatter::TimestampFrequency()) * 1000000.0;

                auto const scaled = (microseconds * SmfTicksPerQuarterNote) / SmfMicrosecondsPerQuarterNote;

                auto tick = scaled >= 4294967040.0 ? 0xFFFFFF00u : static_cast<uint32_t>(scaled + 0.5);

                // Arrival order is the truth here, so a tick may never move backwards even if a
                // timestamp does.
                if (tick < previousTick)
                {
                    tick = previousTick;
                }

                previousTick = tick;

                auto const track = (record.HasGroup && record.GroupNumber >= 1 && record.GroupNumber <= 16)
                    ? trackForGroup[record.GroupNumber - 1]
                    : ungroupedTrack;

                auto const count = record.WordCount > 4 ? uint8_t{ 4 } : record.WordCount;

                builder.AddMessages(
                    track,
                    tick,
                    winrt::array_view<uint32_t const>{ record.Words.data(), record.Words.data() + count });
            }

            return builder.GetSequence();
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnExportClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        ExportAsync();
    }

    winrt::fire_and_forget MainWindow::ExportAsync()
    {
        auto lifetime = get_strong();

        try
        {
            winrt::Windows::Storage::Pickers::FileSavePicker picker{};

            // unpackaged apps have no implicit window association, so give the picker ours
            auto initializeWithWindow = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializeWithWindow->Initialize(WindowHandle()));

            picker.SuggestedStartLocation(winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary);
            picker.SuggestedFileName(res::GetString(L"ExportDefaultFileName"));
            picker.DefaultFileExtension(L".txt");

            picker.FileTypeChoices().Insert(
                res::GetString(L"ExportFileTypeText"),
                winrt::single_threaded_vector<winrt::hstring>({ L".txt" }));
            picker.FileTypeChoices().Insert(
                res::GetString(L"ExportFileTypeCommaSeparated"),
                winrt::single_threaded_vector<winrt::hstring>({ L".csv" }));
            picker.FileTypeChoices().Insert(
                res::GetString(L"ExportFileTypeStandardMidiFile"),
                winrt::single_threaded_vector<winrt::hstring>({ L".mid" }));

            auto const file = co_await picker.PickSaveFileAsync();

            if (file == nullptr)
            {
                co_return;
            }

            auto const path = file.Path();
            auto const format = FormatFromFileType(file.FileType());
            auto const timestampFormat = native::AppSettings::Current().TimestampFormat();
            auto const endpointName = m_monitoredEndpointName;
            auto records = m_pipeline.CopyRetainedRecords();
            auto savedCount = records.size();
            auto queue = m_dispatcherQueue;

            co_await winrt::resume_background();

            auto result = E_FAIL;
            auto nothingToWrite = false;

            if (format == ExportFormat::StandardMidiFile)
            {
                try
                {
                    auto const sequence = BuildSequenceFromCapture(records, endpointName);

                    savedCount = sequence.EventCount();

                    auto const written = co_await files::MidiStandardFileWriter::WriteToFileAsync(file, sequence);

                    if (written != nullptr && written.Succeeded())
                    {
                        result = S_OK;
                    }
                    else if (written != nullptr && written.Status() == files::MidiFileWriteStatus::NothingToWrite)
                    {
                        nothingToWrite = true;
                    }
                }
                catch (...)
                {
                    LOG_CAUGHT_EXCEPTION();
                }
            }
            else
            {
                result = WriteTextExportFile(path, records, timestampFormat, format);
            }

            records.clear();

            if (queue == nullptr)
            {
                co_return;
            }

            queue.TryEnqueue([lifetime, result, savedCount, nothingToWrite, path]()
                {
                    if (SUCCEEDED(result))
                    {
                        lifetime->ShowMessageAsync(
                            res::GetString(L"ExportCompleteTitle"),
                            res::FormatString(L"ExportCompleteBodyFormat", savedCount, std::wstring{ path }));
                    }
                    else
                    {
                        lifetime->ShowMessageAsync(
                            res::GetString(L"ExportFailedTitle"),
                            res::GetString(nothingToWrite ? L"ExportNothingToWriteBody" : L"ExportFailedBody"));
                    }
                });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to export the capture.")
    }
}
