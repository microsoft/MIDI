// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Export of the full capture, including messages that are filtered out of the display.
//

#include "pch.h"
#include "MainWindow.xaml.h"

#include "MidiMessageDecoder.h"
#include "StringResources.h"
#include "TimestampFormatter.h"

namespace native = ::midi2monitor;
namespace res = ::midi2monitor::resources;

namespace winrt::midi2monitor::implementation
{
    namespace
    {
        constexpr size_t ExportFlushThresholdCharacters = 1u << 16;

        void AppendField(std::wstring& line, std::wstring_view value)
        {
            line.append(value);
            line.append(L"\t");
        }

        std::wstring BuildExportHeader()
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

            auto decoded = std::wstring{ native::DecodeMessage(record).ToDisplayString() };
            std::replace(decoded.begin(), decoded.end(), L'\t', L' ');

            line.append(decoded);

            if (!record.Comment.empty())
            {
                auto comment = std::wstring{ record.Comment };
                std::replace(comment.begin(), comment.end(), L'\t', L' ');
                std::replace(comment.begin(), comment.end(), L'\r', L' ');
                std::replace(comment.begin(), comment.end(), L'\n', L' ');

                line.append(L"\t# ");
                line.append(comment);
            }

            line.append(L"\r\n");

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
        HRESULT WriteExportFile(
            winrt::hstring const& path,
            std::vector<native::MessageRecord> const& records,
            native::TimestampDisplayFormat timestampFormat) noexcept
        {
            try
            {
                wil::unique_hfile file{ ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };

                RETURN_LAST_ERROR_IF(!file);

                static constexpr char utf8ByteOrderMark[]{ "\xEF\xBB\xBF" };
                DWORD written{ 0 };
                RETURN_LAST_ERROR_IF(!::WriteFile(file.get(), utf8ByteOrderMark, 3, &written, nullptr));

                std::wstring buffer{ BuildExportHeader() };

                for (auto const& record : records)
                {
                    buffer.append(BuildExportLine(record, timestampFormat));

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
            picker.FileTypeChoices().Insert(
                res::GetString(L"ExportFileTypeText"),
                winrt::single_threaded_vector<winrt::hstring>({ L".txt" }));

            auto const file = co_await picker.PickSaveFileAsync();

            if (file == nullptr)
            {
                co_return;
            }

            auto const path = file.Path();
            auto const timestampFormat = native::AppSettings::Current().TimestampFormat();
            auto records = m_pipeline.CopyRetainedRecords();
            auto const recordCount = records.size();
            auto queue = m_dispatcherQueue;

            co_await winrt::resume_background();

            auto const result = WriteExportFile(path, records, timestampFormat);

            records.clear();

            if (queue == nullptr)
            {
                co_return;
            }

            queue.TryEnqueue([lifetime, result, recordCount, path]()
                {
                    if (SUCCEEDED(result))
                    {
                        lifetime->ShowMessageAsync(
                            res::GetString(L"ExportCompleteTitle"),
                            res::FormatString(L"ExportCompleteBodyFormat", recordCount, std::wstring{ path }));
                    }
                    else
                    {
                        lifetime->ShowMessageAsync(
                            res::GetString(L"ExportFailedTitle"),
                            res::GetString(L"ExportFailedBody"));
                    }
                });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to export the capture.")
    }
}
