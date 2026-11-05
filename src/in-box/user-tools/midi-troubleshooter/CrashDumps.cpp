// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CrashDumps.h"

#include "StringResources.h"
#include "ZipArchive.h"

#include <filesystem>

namespace res = ::miditroubleshooter::resources;

namespace miditroubleshooter::crashdumps
{
    namespace
    {
        constexpr wchar_t LocalDumpsKey[] = LR"(SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\)";

        // WER gives up on a dump long before this, even a large one on a slow disk.
        constexpr DWORD WerFaultWaitMilliseconds = 3 * 60 * 1000;

        // WerFault closes the dump as it exits, but the file system can take a moment after that.
        constexpr DWORD DumpReadableWaitMilliseconds = 5000;

        constexpr DWORD PollMilliseconds = 250;

        std::wstring Lowered(_In_ std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

            return value;
        }

        std::wstring ExecutableStem(_In_ std::wstring const& executableName)
        {
            return std::filesystem::path{ executableName }.stem().wstring();
        }

        uint64_t FileTimeValue(_In_ FILETIME const& time) noexcept
        {
            return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        }

        // A full path, with any "..", links or short names resolved, so two spellings of one
        // folder compare equal.
        std::wstring CanonicalPath(_In_ std::wstring const& path) noexcept
        {
            try
            {
                std::error_code error{};
                auto const canonical = std::filesystem::weakly_canonical(std::filesystem::path{ path }, error);

                return error ? std::wstring{} : canonical.wstring();
            }
            catch (...)
            {
                return {};
            }
        }

        bool IsUnder(_In_ std::wstring const& path, _In_ std::wstring const& folder) noexcept
        {
            try
            {
                auto const canonicalPath = Lowered(CanonicalPath(path));
                auto canonicalFolder = Lowered(CanonicalPath(folder));

                if (canonicalPath.empty() || canonicalFolder.empty())
                {
                    return false;
                }

                if (canonicalFolder.back() != L'\\')
                {
                    canonicalFolder.push_back(L'\\');
                }

                return canonicalPath.size() > canonicalFolder.size() &&
                    canonicalPath.compare(0, canonicalFolder.size(), canonicalFolder) == 0;
            }
            catch (...)
            {
                return false;
            }
        }

        // The id came from another process, so it has to be the WerFault.exe it says it is.
        wil::unique_handle OpenWerFault(_In_ DWORD processId) noexcept
        {
            if (processId == 0)
            {
                return {};
            }

            wil::unique_handle process{ ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };

            if (!process)
            {
                return {};
            }

            wchar_t imagePath[MAX_PATH]{};
            DWORD imagePathLength{ ARRAYSIZE(imagePath) };

            if (!::QueryFullProcessImageNameW(process.get(), 0, imagePath, &imagePathLength))
            {
                return {};
            }

            auto const fileName = ::PathFindFileNameW(imagePath);

            if (_wcsicmp(fileName, L"WerFault.exe") != 0)
            {
                return {};
            }

            return process;
        }

        // Waits in short steps so the caller can give up.
        bool WaitForProcessExit(_In_ HANDLE process, _In_ DWORD timeoutMilliseconds, _In_ std::atomic<bool> const& cancel) noexcept
        {
            auto const started = ::GetTickCount64();

            while (::GetTickCount64() - started < timeoutMilliseconds)
            {
                if (cancel.load())
                {
                    return false;
                }

                if (::WaitForSingleObject(process, PollMilliseconds) == WAIT_OBJECT_0)
                {
                    return true;
                }
            }

            return false;
        }

        bool IsReadable(_In_ std::wstring const& path) noexcept
        {
            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            return static_cast<bool>(file);
        }

        std::wstring ServiceDumpFolder()
        {
            auto const root = RootFolder();

            return root.empty() ? std::wstring{} : root + L"\\" + ServiceDumpFolderName;
        }

        void AddDumpsIn(_In_ std::filesystem::path const& folder, _Inout_ std::vector<DumpFile>& dumps)
        {
            std::error_code error{};

            for (auto const& entry : std::filesystem::directory_iterator{ folder, std::filesystem::directory_options::skip_permission_denied, error })
            {
                std::error_code entryError{};

                if (!entry.is_regular_file(entryError) || entryError)
                {
                    continue;
                }

                if (Lowered(entry.path().extension().wstring()) != L".dmp")
                {
                    continue;
                }

                WIN32_FILE_ATTRIBUTE_DATA attributes{};

                if (!::GetFileAttributesExW(entry.path().c_str(), GetFileExInfoStandard, &attributes))
                {
                    continue;
                }

                DumpFile dump{};
                dump.Path = entry.path().wstring();
                dump.FileName = entry.path().filename().wstring();
                dump.FolderName = folder.filename().wstring();
                dump.Size = (static_cast<uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
                dump.WriteTime = FileTimeValue(attributes.ftLastWriteTime);
                dump.IsServiceDump = _wcsicmp(dump.FolderName.c_str(), ServiceDumpFolderName) == 0;

                dumps.push_back(std::move(dump));
            }
        }

        // Keeps the newest few, the way WER does for the tools' own dumps.
        void TrimServiceDumps(_In_ std::wstring const& folder) noexcept
        {
            try
            {
                std::vector<DumpFile> dumps{};
                AddDumpsIn(folder, dumps);

                if (dumps.size() <= MaximumServiceDumps)
                {
                    return;
                }

                std::sort(dumps.begin(), dumps.end(), [](DumpFile const& a, DumpFile const& b) { return a.WriteTime > b.WriteTime; });

                for (size_t i = MaximumServiceDumps; i < dumps.size(); i++)
                {
                    LOG_IF_WIN32_BOOL_FALSE(::DeleteFileW(dumps[i].Path.c_str()));
                }
            }
            MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to remove old MIDI service dumps.")
        }

        std::wstring Timestamp()
        {
            SYSTEMTIME now{};
            ::GetLocalTime(&now);

            return std::format(L"{:04}-{:02}-{:02}_{:02}-{:02}-{:02}",
                now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
        }
    }

    std::wstring RootFolder() noexcept
    {
        try
        {
            wil::unique_cotaskmem_string localAppData{};

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, localAppData.put())))
            {
                return {};
            }

            return std::wstring{ localAppData.get() } + LR"(\Microsoft\MIDI\CrashDumps)";
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to find the crash dump folder.")

        return {};
    }

    _Use_decl_annotations_
    bool HasLocalDumpsEntry(std::wstring const& executableName) noexcept
    {
        try
        {
            wil::unique_hkey key{};

            return ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LocalDumpsKey + executableName).c_str(), 0, KEY_READ, key.put()) == ERROR_SUCCESS;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring ToolDumpFolder(std::wstring const& executableName) noexcept
    {
        try
        {
            wchar_t raw[MAX_PATH]{};
            DWORD size{ sizeof(raw) };

            // REG_EXPAND_SZ, because %LOCALAPPDATA% has to be the crashing user's folder
            if (::RegGetValueW(
                HKEY_LOCAL_MACHINE,
                (LocalDumpsKey + executableName).c_str(),
                L"DumpFolder",
                RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
                nullptr,
                raw,
                &size) == ERROR_SUCCESS)
            {
                wchar_t expanded[MAX_PATH]{};
                auto const length = ::ExpandEnvironmentStringsW(raw, expanded, ARRAYSIZE(expanded));

                if (length > 0 && length <= ARRAYSIZE(expanded))
                {
                    return std::wstring{ expanded };
                }
            }

            auto const root = RootFolder();

            return root.empty() ? std::wstring{} : root + L"\\" + ExecutableStem(executableName);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to find a tool's crash dump folder.")

        return {};
    }

    std::vector<DumpFile> ListDumps() noexcept
    {
        std::vector<DumpFile> dumps{};

        try
        {
            auto const root = RootFolder();

            if (root.empty())
            {
                return dumps;
            }

            std::error_code error{};

            for (auto const& entry : std::filesystem::directory_iterator{ root, std::filesystem::directory_options::skip_permission_denied, error })
            {
                std::error_code entryError{};

                if (entry.is_directory(entryError) && !entryError)
                {
                    AddDumpsIn(entry.path(), dumps);
                }
            }

            std::sort(dumps.begin(), dumps.end(), [](DumpFile const& a, DumpFile const& b) { return a.WriteTime > b.WriteTime; });
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to list the crash dumps.")

        return dumps;
    }

    _Use_decl_annotations_
    CrashDumpWaitResult WaitForCrashDump(
        DWORD const crashedProcessId,
        std::wstring const& executableName,
        DWORD const werFaultProcessId,
        std::atomic<bool> const& cancel) noexcept
    {
        CrashDumpWaitResult result{};

        try
        {
            auto const folder = ToolDumpFolder(executableName);

            if (folder.empty())
            {
                return result;
            }

            // WER's own name for the file
            auto const path = std::format(L"{}\\{}.{}.dmp", folder, executableName, crashedProcessId);

            // WerFault holds the dump open until it exits, and the crashed process ends at the same
            // moment. When WerFault can't be found, the crashed process is the next best thing.
            auto process = OpenWerFault(werFaultProcessId);

            if (!process)
            {
                process.reset(::OpenProcess(SYNCHRONIZE, FALSE, crashedProcessId));
            }

            if (process && !WaitForProcessExit(process.get(), WerFaultWaitMilliseconds, cancel))
            {
                result.Outcome = cancel.load() ? CrashDumpWait::Canceled : CrashDumpWait::NotSaved;
                return result;
            }

            auto const started = ::GetTickCount64();

            while (::GetTickCount64() - started < DumpReadableWaitMilliseconds)
            {
                if (cancel.load())
                {
                    result.Outcome = CrashDumpWait::Canceled;
                    return result;
                }

                if (IsReadable(path))
                {
                    result.Outcome = CrashDumpWait::Saved;
                    result.Path = path;
                    return result;
                }

                ::Sleep(PollMilliseconds);
            }
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to wait for a crash dump.")

        return result;
    }

    _Use_decl_annotations_
    ServiceDumpResult SaveServiceDump(std::wstring const& crashedExecutable, DWORD const crashedProcessId) noexcept
    {
        ServiceDumpResult result{};

        try
        {
            auto const folder = ServiceDumpFolder();

            if (folder.empty())
            {
                result.Operation.ErrorMessage = std::wstring{ res::GetString(L"CrashDumpsNoFolder") };
                return result;
            }

            auto const created = ::SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);

            if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS)
            {
                result.Operation.ErrorMessage = std::wstring{ res::GetString(L"CrashDumpsNoFolder") };
                return result;
            }

            // Named after the tool that crashed, so the two dumps sort and read as a pair.
            auto const fileName = (crashedExecutable.empty() || crashedProcessId == 0) ?
                std::format(L"midisrv.{}.dmp", Timestamp()) :
                std::format(L"midisrv.{}.after.{}.{}.dmp", Timestamp(), crashedExecutable, crashedProcessId);

            result.Path = folder + L"\\" + fileName;
            result.Operation = WriteMidiServiceDump(result.Path);

            if (result.Operation.Succeeded)
            {
                TrimServiceDumps(folder);
            }
            else
            {
                // a dump that failed part way would only take up space
                ::DeleteFileW(result.Path.c_str());
                result.Path.clear();
            }
        }
        catch (...)
        {
            result.Operation.Succeeded = false;
            result.Operation.ErrorMessage = std::wstring{ res::GetString(L"CrashDumpsServiceDumpFailed") };
        }

        return result;
    }

    _Use_decl_annotations_
    bool DeleteDump(std::wstring const& path) noexcept
    {
        try
        {
            auto const root = RootFolder();

            if (root.empty() || !IsUnder(path, root) || Lowered(std::filesystem::path{ path }.extension().wstring()) != L".dmp")
            {
                return false;
            }

            return ::DeleteFileW(path.c_str()) != FALSE;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to delete a crash dump.")

        return false;
    }

    _Use_decl_annotations_
    bool ZipDumps(std::vector<DumpFile> const& dumps, std::wstring const& zipPath, std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
            midiapp::ZipWriter zip{};

            auto status = zip.Create(zipPath);

            for (auto const& dump : dumps)
            {
                if (status != midiapp::ZipStatus::Written)
                {
                    break;
                }

                status = zip.AddFile(dump.FolderName + L"/" + dump.FileName, dump.Path, midiapp::ZipCompression::Deflate);
            }

            if (status == midiapp::ZipStatus::Written)
            {
                status = zip.Finish();
            }

            if (status == midiapp::ZipStatus::Written)
            {
                return true;
            }

            // A zip without zip64 can't go past 4 GB, which a handful of dumps can.
            errorMessage = std::wstring{ res::GetString(status == midiapp::ZipStatus::TooLarge ?
                L"CrashDumpsZipTooLarge" : L"CrashDumpsZipFailed") };
        }
        catch (...)
        {
            errorMessage = std::wstring{ res::GetString(L"CrashDumpsZipFailed") };
        }

        return false;
    }

    _Use_decl_annotations_
    bool OpenFolderInExplorer(std::wstring const& folder) noexcept
    {
        try
        {
            if (folder.empty())
            {
                return false;
            }

            // WER makes a tool's folder on its first crash, so it may not be there yet.
            ::SHCreateDirectoryExW(nullptr, folder.c_str(), nullptr);

            return reinterpret_cast<INT_PTR>(::ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to open the crash dump folder.")

        return false;
    }

    _Use_decl_annotations_
    bool ShowFileInExplorer(std::wstring const& path) noexcept
    {
        try
        {
            auto const item = ::ILCreateFromPathW(path.c_str());

            if (item == nullptr)
            {
                return false;
            }

            auto const hr = ::SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
            ::ILFree(item);

            return SUCCEEDED(hr);
        }
        MIDI_TSHOOT_CATCH_AND_LOG(L"Unable to show a crash dump.")

        return false;
    }
}
