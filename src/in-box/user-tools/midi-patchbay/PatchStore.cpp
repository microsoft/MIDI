// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "PatchStore.h"
#include "CapabilityInquiry.h"
#include "PatchFolderMove.h"
#include "PatchSerializer.h"
#include "StringResources.h"

namespace midipatchbay
{
    namespace
    {
        constexpr wchar_t FolderName[] = L"MIDI Patches";

        // Where earlier versions kept patches. Its contents move to FolderName.
        constexpr wchar_t PreviousFolderName[] = L"MIDI Patchbay";

        // Inside the patch folder. The app only reads the files directly in that folder, so
        // nothing in here is ever loaded as a patch by mistake.
        constexpr wchar_t EarlierVersionsFolderName[] = L"Earlier versions";

        std::wstring Utf8ToWide(_In_ std::string const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

            if (required <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(required), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required);

            return result;
        }

        std::string WideToUtf8(_In_ std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const required = ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (required <= 0)
            {
                return {};
            }

            std::string result(static_cast<size_t>(required), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                result.data(), required, nullptr, nullptr);

            return result;
        }

        bool ReadAllBytes(_In_ std::wstring const& path, _Out_ std::string& contents) noexcept
        {
            contents.clear();

            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return false;
            }

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 ||
                static_cast<uint64_t>(size.QuadPart) > MaximumPatchFileBytes)
            {
                return false;
            }

            if (size.QuadPart == 0)
            {
                return true;
            }

            contents.resize(static_cast<size_t>(size.QuadPart));

            DWORD bytesRead{ 0 };

            if (!::ReadFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &bytesRead, nullptr))
            {
                contents.clear();
                return false;
            }

            contents.resize(bytesRead);

            // a UTF-8 byte order mark is legal in the file but not in the JSON text
            if (contents.size() >= 3 &&
                static_cast<unsigned char>(contents[0]) == 0xEF &&
                static_cast<unsigned char>(contents[1]) == 0xBB &&
                static_cast<unsigned char>(contents[2]) == 0xBF)
            {
                contents.erase(0, 3);
            }

            return true;
        }

        bool WriteAllBytes(_In_ std::wstring const& path, _In_ std::string const& contents) noexcept
        {
            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_WRITE,
                FILE_SHARE_READ,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return false;
            }

            if (contents.empty())
            {
                return true;
            }

            DWORD written{ 0 };

            if (!::WriteFile(file.get(), contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr))
            {
                return false;
            }

            return written == contents.size();
        }

        // Seconds since 1970, not a FILETIME: a FILETIME needs more bits than a JSON number can
        // hold exactly, so it comes back rounded to the nearest few seconds.
        int64_t CurrentTimestamp() noexcept
        {
            FILETIME now{};
            ::GetSystemTimeAsFileTime(&now);

            ULARGE_INTEGER value{};
            value.LowPart = now.dwLowDateTime;
            value.HighPart = now.dwHighDateTime;

            constexpr uint64_t HundredNanosecondsPerSecond = 10000000ull;
            constexpr uint64_t SecondsFrom1601To1970 = 11644473600ull;

            return static_cast<int64_t>(value.QuadPart / HundredNanosecondsPerSecond) -
                static_cast<int64_t>(SecondsFrom1601To1970);
        }

        // A patch name is customer text and goes straight into a file name, so everything the
        // file system reserves is replaced rather than escaped.
        std::wstring MakeSafeFileStem(_In_ std::wstring const& name) noexcept
        {
            std::wstring result{};
            result.reserve(name.size());

            for (auto const ch : name)
            {
                if (ch < L' ' || ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                    ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
                {
                    result.push_back(L'_');
                }
                else
                {
                    result.push_back(ch);
                }
            }

            // trailing dots and spaces are legal to type but cannot be created on disk
            while (!result.empty() && (result.back() == L'.' || result.back() == L' '))
            {
                result.pop_back();
            }

            if (result.size() > 96)
            {
                result.resize(96);
            }

            if (result.empty())
            {
                result = L"Patch";
            }

            // the reserved DOS device names are still reserved with an extension attached
            static constexpr std::wstring_view reserved[] = {
                L"CON", L"PRN", L"AUX", L"NUL",
                L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
                L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9" };

            std::wstring upper{ result };
            std::transform(upper.begin(), upper.end(), upper.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });

            for (auto const& name2 : reserved)
            {
                if (upper == name2)
                {
                    result.insert(result.begin(), L'_');
                    break;
                }
            }

            return result;
        }
    }

    PatchStore::PatchStore() noexcept
    {
        try
        {
            wil::unique_cotaskmem_string documents;

            if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)) &&
                documents)
            {
                std::filesystem::path const root{ documents.get() };
                auto const previous = (root / PreviousFolderName).wstring();

                m_folder = MoveEarlierPatchFolder(previous, (root / FolderName).wstring());

                if (m_folder == previous)
                {
                    MIDI_PATCHBAY_LOG_WARNING(L"The earlier patch folder could not be moved yet, so it is still the one in use.");
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to resolve the patch folder.")
    }

    PatchStore& PatchStore::Current() noexcept
    {
        static PatchStore instance{};
        return instance;
    }

    bool PatchStore::EnsureFolder() noexcept
    {
        try
        {
            if (m_folder.empty())
            {
                m_lastError = resources::GetString(L"ErrorNoPatchFolder");
                return false;
            }

            std::error_code ec{};

            if (std::filesystem::exists(m_folder, ec))
            {
                return true;
            }

            std::filesystem::create_directories(m_folder, ec);

            if (ec)
            {
                m_lastError = resources::FormatString(L"ErrorCreateFolderFormat", m_folder);
                return false;
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to create the patch folder.")

        m_lastError = resources::GetString(L"ErrorNoPatchFolder");
        return false;
    }

    void PatchStore::ShowFolder() noexcept
    {
        try
        {
            if (!EnsureFolder())
            {
                return;
            }

            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOASYNC;
            info.lpVerb = L"open";
            info.lpFile = m_folder.c_str();
            info.nShow = SW_SHOWNORMAL;

            ::ShellExecuteExW(&info);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to open the patch folder.")
    }

    _Use_decl_annotations_
    std::wstring PatchStore::BuildUniqueFilePath(
        std::wstring const& patchName,
        std::wstring const& currentPath) const noexcept
    {
        try
        {
            if (m_folder.empty())
            {
                return {};
            }

            auto const stem = MakeSafeFileStem(patchName);

            for (int suffix = 0; suffix < 1000; suffix++)
            {
                std::filesystem::path candidate{ m_folder };

                candidate /= suffix == 0
                    ? stem + FileExtension
                    : stem + L" (" + std::to_wstring(suffix) + L")" + FileExtension;

                auto const text = candidate.wstring();

                if (!currentPath.empty() && ::CompareStringOrdinal(
                    text.c_str(), -1, currentPath.c_str(), -1, TRUE) == CSTR_EQUAL)
                {
                    return text;
                }

                std::error_code ec{};

                if (!std::filesystem::exists(candidate, ec))
                {
                    return text;
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to build a patch file name.")

        return {};
    }

    _Use_decl_annotations_
    std::optional<PatchDocument> PatchStore::LoadFile(std::wstring const& path) noexcept
    {
        try
        {
            std::string bytes{};

            if (!ReadAllBytes(path, bytes) || bytes.empty())
            {
                return std::nullopt;
            }

            auto patch = ReadPatchJson(Utf8ToWide(bytes), std::filesystem::path{ path }.stem().wstring());

            if (patch.has_value())
            {
                patch->FilePath = path;
            }

            return patch;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read a patch file.")

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool PatchStore::KeepEarlierVersion(PatchDocument& patch) noexcept
    {
        try
        {
            std::filesystem::path const original{ patch.FilePath };
            std::filesystem::path const folder = std::filesystem::path{ m_folder } / EarlierVersionsFolderName;

            std::error_code ec{};
            std::filesystem::create_directories(folder, ec);

            if (ec)
            {
                return false;
            }

            auto const stem = original.stem().wstring();
            auto const extension = original.extension().wstring();

            // Never over another copy: converting twice must not lose the first original.
            for (int suffix = 0; suffix < 1000; suffix++)
            {
                auto const target = folder / (suffix == 0
                    ? stem + extension
                    : stem + L" (" + std::to_wstring(suffix) + L")" + extension);

                if (::CopyFileW(original.c_str(), target.c_str(), TRUE))
                {
                    patch.EarlierVersionPath = target.wstring();
                    return true;
                }

                if (::GetLastError() != ERROR_FILE_EXISTS)
                {
                    return false;
                }
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to keep a copy of an earlier patch file.")

        return false;
    }

    _Use_decl_annotations_
    bool PatchStore::LoadAll(std::vector<PatchDocument>& patches) noexcept
    {
        patches.clear();

        try
        {
            if (m_folder.empty())
            {
                m_lastError = resources::GetString(L"ErrorNoPatchFolder");
                return false;
            }

            std::error_code ec{};

            if (!std::filesystem::exists(m_folder, ec))
            {
                return true;
            }

            RenameLegacyFiles();

            for (auto const& entry : std::filesystem::directory_iterator{ m_folder, ec })
            {
                if (ec)
                {
                    break;
                }

                if (patches.size() >= MaximumPatchCount)
                {
                    break;
                }

                if (!entry.is_regular_file(ec))
                {
                    continue;
                }

                if (!IsPatchFileName(entry.path().filename().wstring()))
                {
                    continue;
                }

                auto loaded = LoadFile(entry.path().wstring());

                if (!loaded.has_value())
                {
                    continue;
                }

                // A file an earlier version wrote is converted as it is read. The original is
                // kept first, and only once it is safe is the file rewritten in the new form.
                if (loaded->LoadedFileVersion < CurrentPatchFileVersion && KeepEarlierVersion(loaded.value()))
                {
                    Save(loaded.value());
                }

                patches.push_back(std::move(loaded.value()));
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to enumerate the patch folder.")

        m_lastError = resources::GetString(L"ErrorReadPatches");
        return false;
    }

    _Use_decl_annotations_
    bool PatchStore::IsPatchFileName(std::wstring_view fileName) noexcept
    {
        for (std::wstring_view const extension : { std::wstring_view{ FileExtension }, std::wstring_view{ LegacyFileExtension } })
        {
            if (fileName.size() > extension.size() && ::CompareStringOrdinal(
                fileName.data() + (fileName.size() - extension.size()), static_cast<int>(extension.size()),
                extension.data(), static_cast<int>(extension.size()), TRUE) == CSTR_EQUAL)
            {
                return true;
            }
        }

        return false;
    }

    void PatchStore::RenameLegacyFiles() noexcept
    {
        try
        {
            std::error_code ec{};
            std::vector<std::filesystem::path> found{};

            std::wstring_view const legacy{ LegacyFileExtension };

            // Collected before any rename, so the walk never meets a file it has just renamed.
            for (auto const& entry : std::filesystem::directory_iterator{ m_folder, ec })
            {
                auto const name = entry.path().filename().wstring();

                if (entry.is_regular_file(ec) && name.size() > legacy.size() && ::CompareStringOrdinal(
                    name.c_str() + (name.size() - legacy.size()), static_cast<int>(legacy.size()),
                    legacy.data(), static_cast<int>(legacy.size()), TRUE) == CSTR_EQUAL)
                {
                    found.push_back(entry.path());
                }
            }

            for (auto const& from : found)
            {
                auto const name = from.filename().wstring();
                auto const to = from.parent_path() / (name.substr(0, name.size() - legacy.size()) + FileExtension);

                // No replace flag: a file already holding the new name is somebody's, and stays.
                ::MoveFileExW(from.c_str(), to.c_str(), 0);
            }
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to rename the older patch files.")
    }

    _Use_decl_annotations_
    std::optional<PatchDocument> PatchStore::Import(std::wstring const& sourcePath) noexcept
    {
        try
        {
            auto const fileName = std::filesystem::path{ sourcePath }.filename().wstring();

            std::error_code ec{};

            if (sourcePath.empty() || !IsPatchFileName(fileName) ||
                !std::filesystem::is_regular_file(sourcePath, ec))
            {
                m_lastError = resources::FormatString(L"ErrorImportPatchFormat", fileName);
                return std::nullopt;
            }

            auto patch = LoadFile(sourcePath);

            if (!patch.has_value())
            {
                m_lastError = resources::FormatString(L"ErrorImportPatchFormat", fileName);
                return std::nullopt;
            }

            if (!EnsureFolder())
            {
                return std::nullopt;
            }

            auto const inFolder = std::filesystem::equivalent(std::filesystem::path{ sourcePath }.parent_path(), m_folder, ec);

            // A newer version's patch holds what this one can't read, so it is copied as it is and
            // never written again from the part this version understood.
            if (patch->IsFromNewerVersion)
            {
                patch->ActivateAtStartup = false;

                if (inFolder)
                {
                    return patch;
                }

                auto const target = BuildUniqueFilePath(patch->Name, {});

                if (target.empty() || !::CopyFileW(sourcePath.c_str(), target.c_str(), TRUE))
                {
                    m_lastError = resources::FormatString(L"ErrorSavePatchFormat", target);
                    return std::nullopt;
                }

                patch->FilePath = target;
                patch->IsTemporary = false;

                return patch;
            }

            if (inFolder)
            {
                if (patch->LoadedFileVersion < CurrentPatchFileVersion && KeepEarlierVersion(patch.value()))
                {
                    Save(patch.value());
                }

                return patch;
            }

            // Written by this app rather than copied, so the file in the folder is one it wrote
            // itself, under a name that is free, whatever the other file was called.
            patch->FilePath.clear();
            patch->CreatedTimestamp = 0;

            // Somebody else wrote this file, so it does not start routing by itself the next time
            // the app starts either. The customer chooses that in the app.
            patch->ActivateAtStartup = false;

            // A MIDI-CI responder's file comes too, from beside the patch. One already in the
            // folder by that name is the customer's, and is left as it is.
            for (auto const& block : patch->Blocks)
            {
                auto const& name = block.Settings.CiResponder.FileName;

                if (block.Kind != BlockKind::CiResponder || !IsCiFileName(name))
                {
                    continue;
                }

                auto const from = std::filesystem::path{ sourcePath }.parent_path() / name;
                auto const to = std::filesystem::path{ m_folder } / name;

                if (std::filesystem::is_regular_file(from, ec) && !std::filesystem::exists(to, ec) &&
                    std::filesystem::file_size(from, ec) <= MaximumCiFileBytes)
                {
                    std::filesystem::copy_file(from, to, ec);
                }
            }

            if (!Save(patch.value()))
            {
                return std::nullopt;
            }

            return patch;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to import a patch.")

        m_lastError = resources::GetString(L"ErrorSavePatch");
        return std::nullopt;
    }

    _Use_decl_annotations_
    bool PatchStore::Save(PatchDocument& patch) noexcept
    {
        try
        {
            // Writing it would throw away everything in it this version couldn't read.
            if (patch.IsFromNewerVersion)
            {
                m_lastError = resources::GetString(L"ErrorSaveNewerPatch");
                return false;
            }

            if (!EnsureFolder())
            {
                return false;
            }

            auto const targetPath = BuildUniqueFilePath(patch.Name, patch.FilePath);

            if (targetPath.empty())
            {
                m_lastError = resources::GetString(L"ErrorNoPatchFolder");
                return false;
            }

            if (patch.CreatedTimestamp == 0)
            {
                patch.CreatedTimestamp = CurrentTimestamp();
            }

            patch.ModifiedTimestamp = CurrentTimestamp();

            auto const text = WritePatchJson(patch);

            if (text.empty() || !WriteAllBytes(targetPath, WideToUtf8(text)))
            {
                m_lastError = resources::FormatString(L"ErrorSavePatchFormat", targetPath);
                return false;
            }

            // a rename leaves the previous file behind otherwise
            if (!patch.FilePath.empty() && ::CompareStringOrdinal(
                patch.FilePath.c_str(), -1, targetPath.c_str(), -1, TRUE) != CSTR_EQUAL)
            {
                std::error_code ec{};
                std::filesystem::remove(patch.FilePath, ec);
            }

            patch.FilePath = targetPath;
            patch.IsTemporary = false;

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to save the patch.")

        m_lastError = resources::GetString(L"ErrorSavePatch");
        return false;
    }

    _Use_decl_annotations_
    bool PatchStore::Delete(PatchDocument const& patch) noexcept
    {
        try
        {
            if (patch.FilePath.empty())
            {
                return true;
            }

            std::error_code ec{};
            std::filesystem::remove(patch.FilePath, ec);

            if (ec)
            {
                m_lastError = resources::FormatString(L"ErrorDeletePatchFormat", patch.FilePath);
                return false;
            }

            return true;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to delete the patch.")

        m_lastError = resources::GetString(L"ErrorDeletePatch");
        return false;
    }
}
