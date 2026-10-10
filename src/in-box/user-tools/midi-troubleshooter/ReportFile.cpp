// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ReportFile.h"

#include "ProcessRunner.h"
#include "StringResources.h"
#include "ToolPaths.h"
#include "ZipArchive.h"

namespace res = ::miditroubleshooter::resources;
namespace rpt = ::mididiag::report;

namespace miditroubleshooter
{
    namespace
    {
        // A support package holds a handful of text files. A zip with hundreds wasn't made for a
        // MIDI problem, so only the likeliest few are tried.
        constexpr size_t MaximumZipEntriesTried{ 20 };

        // every zip that doesn't need zip64
        constexpr uint32_t MaximumZipEntries{ 0xFFFF };

        constexpr size_t ReadChunkBytes{ 1024 * 1024 };

        bool EqualsNoCase(_In_ std::wstring_view const left, _In_ std::wstring_view const right) noexcept
        {
            return ::CompareStringOrdinal(
                left.data(), static_cast<int>(left.size()),
                right.data(), static_cast<int>(right.size()),
                TRUE) == CSTR_EQUAL;
        }

        bool StartsWithNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const prefix) noexcept
        {
            return text.size() >= prefix.size() && EqualsNoCase(text.substr(0, prefix.size()), prefix);
        }

        bool EndsWithNoCase(_In_ std::wstring_view const text, _In_ std::wstring_view const suffix) noexcept
        {
            return text.size() >= suffix.size() && EqualsNoCase(text.substr(text.size() - suffix.size()), suffix);
        }

        std::wstring_view FileNamePart(_In_ std::wstring_view const path) noexcept
        {
            auto const separator = path.find_last_of(L"\\/");

            return separator == std::wstring_view::npos ? path : path.substr(separator + 1);
        }

        // A zip starts with a file header, or with the end record when it's empty.
        bool IsZipSignature(_In_ std::span<std::byte const> const bytes) noexcept
        {
            return bytes.size() >= 4 &&
                bytes[0] == std::byte{ 0x50 } &&
                bytes[1] == std::byte{ 0x4B } &&
                ((bytes[2] == std::byte{ 0x03 } && bytes[3] == std::byte{ 0x04 }) ||
                 (bytes[2] == std::byte{ 0x05 } && bytes[3] == std::byte{ 0x06 }));
        }

        LoadedReport FromParseResult(_In_ rpt::ParseResult parsed)
        {
            LoadedReport loaded{};

            switch (parsed.Status)
            {
            case rpt::ParseStatus::Succeeded:
                loaded.Report = std::make_shared<rpt::Report>(std::move(parsed.Parsed));
                break;

            case rpt::ParseStatus::Empty:
                loaded.Error = ReportLoadError::Empty;
                break;

            case rpt::ParseStatus::TooLarge:
                loaded.Error = ReportLoadError::TooLarge;
                break;

            case rpt::ParseStatus::NotAReport:
            default:
                loaded.Error = ReportLoadError::NotAReport;
                break;
            }

            return loaded;
        }

        // mididiag.txt is the name both the Troubleshooter and its capture use, so it's tried first.
        int EntryPreference(_In_ std::wstring_view const name) noexcept
        {
            auto const fileName = FileNamePart(name);

            if (EqualsNoCase(fileName, L"mididiag.txt"))
            {
                return 0;
            }

            return StartsWithNoCase(fileName, L"mididiag") ? 1 : 2;
        }

        std::vector<midiapp::ZipItem const*> ReportCandidates(_In_ std::vector<midiapp::ZipItem> const& items)
        {
            std::vector<midiapp::ZipItem const*> candidates{};

            for (auto const& item : items)
            {
                if (EndsWithNoCase(item.Name, L".txt"))
                {
                    candidates.push_back(&item);
                }
            }

            std::stable_sort(candidates.begin(), candidates.end(), [](midiapp::ZipItem const* left, midiapp::ZipItem const* right)
                {
                    return EntryPreference(left->Name) < EntryPreference(right->Name);
                });

            if (candidates.size() > MaximumZipEntriesTried)
            {
                candidates.resize(MaximumZipEntriesTried);
            }

            return candidates;
        }

        LoadedReport LoadFromZip(_In_ std::wstring const& zipPath)
        {
            LoadedReport failed{};

            failed.FilePath = zipPath;
            failed.Error = ReportLoadError::ZipUnreadable;

            midiapp::ZipReader zip{};

            if (zip.OpenFile(zipPath, MaximumZipEntries) != midiapp::ZipStatus::Read)
            {
                return failed;
            }

            for (auto const* item : ReportCandidates(zip.Items()))
            {
                std::vector<uint8_t> bytes{};

                if (zip.Extract(*item, rpt::MaximumReportSize, bytes) != midiapp::ZipStatus::Read)
                {
                    continue;
                }

                auto parsed = rpt::ParseReport(rpt::DecodeReportBytes(std::as_bytes(std::span{ bytes })));

                if (parsed.Status == rpt::ParseStatus::Succeeded)
                {
                    auto loaded = FromParseResult(std::move(parsed));

                    // a zip Windows tar made from a folder lists its files as ./name
                    std::wstring_view shownName{ item->Name };

                    while (shownName.starts_with(L"./"))
                    {
                        shownName.remove_prefix(2);
                    }

                    loaded.FilePath = zipPath;
                    loaded.EntryName = std::wstring{ shownName };

                    return loaded;
                }
            }

            failed.Error = ReportLoadError::ZipHasNoReport;

            return failed;
        }
    }

    _Use_decl_annotations_
    LoadedReport LoadReportText(std::wstring_view const text) noexcept
    {
        try
        {
            return FromParseResult(rpt::ParseReport(text));
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read a report.")

        LoadedReport failed{};
        failed.Error = ReportLoadError::CannotRead;

        return failed;
    }

    _Use_decl_annotations_
    LoadedReport LoadReportFile(std::wstring const& path) noexcept
    {
        LoadedReport loaded{};

        loaded.FilePath = path;
        loaded.Error = ReportLoadError::CannotRead;

        try
        {
            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_FLAG_SEQUENTIAL_SCAN,
                nullptr) };

            LARGE_INTEGER size{};

            if (!file || !::GetFileSizeEx(file.get(), &size) || size.QuadPart < 0)
            {
                return loaded;
            }

            // A zip can be far larger than any report, so only its first bytes are read here.
            std::array<std::byte, 4> signature{};
            DWORD signatureLength{ 0 };

            if (!::ReadFile(file.get(), signature.data(), static_cast<DWORD>(signature.size()), &signatureLength, nullptr))
            {
                return loaded;
            }

            if (signatureLength == signature.size() && IsZipSignature(signature))
            {
                file.reset();

                return LoadFromZip(path);
            }

            if (static_cast<uint64_t>(size.QuadPart) > rpt::MaximumReportSize)
            {
                loaded.Error = ReportLoadError::TooLarge;
                return loaded;
            }

            std::vector<std::byte> bytes(static_cast<size_t>(size.QuadPart));

            auto total = std::min<size_t>(signatureLength, bytes.size());

            std::copy_n(signature.begin(), total, bytes.begin());

            while (total < bytes.size())
            {
                DWORD read{ 0 };

                auto const chunk = static_cast<DWORD>(std::min(bytes.size() - total, ReadChunkBytes));

                if (!::ReadFile(file.get(), bytes.data() + total, chunk, &read, nullptr))
                {
                    return loaded;
                }

                // the file got shorter while it was being read
                if (read == 0)
                {
                    break;
                }

                total += read;
            }

            bytes.resize(total);

            auto result = FromParseResult(rpt::ParseReport(rpt::DecodeReportBytes(bytes)));

            result.FilePath = path;

            return result;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to read a report file.")

        return loaded;
    }

    _Use_decl_annotations_
    std::wstring ShowOpenReportDialog(HWND const owner) noexcept
    {
        try
        {
            winrt::com_ptr<IFileOpenDialog> dialog{};

            if (FAILED(::CoCreateInstance(
                CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            auto const reportsLabel = res::GetString(L"OpenReportFileType");
            auto const allFilesLabel = res::GetString(L"OpenAllFilesType");
            auto const title = res::GetString(L"OpenReportDialogTitle");

            COMDLG_FILTERSPEC const filters[]
            {
                { reportsLabel.c_str(), L"*.txt;*.zip" },
                { allFilesLabel.c_str(), L"*.*" }
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetTitle(title.c_str()));
            LOG_IF_FAILED(dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

            // canceling is reported as a failure hresult, so this is not logged as an error
            if (FAILED(dialog->Show(owner)))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())) || item == nullptr)
            {
                return {};
            }

            wil::unique_cotaskmem_string path{};

            if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, path.put())))
            {
                return {};
            }

            return std::wstring{ path.get() };
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show the open dialog.")

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring ReportLoadErrorMessage(ReportLoadError const error) noexcept
    {
        switch (error)
        {
        case ReportLoadError::Empty:
            return res::GetString(L"ReportLoadEmpty");

        case ReportLoadError::TooLarge:
            return res::GetString(L"ReportLoadTooLarge");

        case ReportLoadError::NotAReport:
            return res::GetString(L"ReportLoadNotAReport");

        case ReportLoadError::ZipUnreadable:
            return res::GetString(L"ReportLoadZipUnreadable");

        case ReportLoadError::ZipHasNoReport:
            return res::GetString(L"ReportLoadZipHasNoReport");

        case ReportLoadError::CannotRead:
        case ReportLoadError::None:
        default:
            return res::GetString(L"ReportLoadCannotRead");
        }
    }
}
