// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "LayoutPackage.h"
#include "LayoutStore.h"
#include "LayoutSerializer.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace glass
{
    namespace
    {
        // ---- the little bit of zip we actually need ----
        //
        // Stored entries only. The whole format used here is three records: a local header in
        // front of each file, a central directory listing them all, and an end record pointing
        // at the directory. No compression, no encryption, no spanning, no zip64.

        constexpr uint32_t LocalHeaderSignature = 0x04034b50;
        constexpr uint32_t CentralHeaderSignature = 0x02014b50;
        constexpr uint32_t EndOfDirectorySignature = 0x06054b50;

        constexpr uint16_t MethodStored = 0;

        // 2.0, which is what every tool expects to see even on a stored entry.
        constexpr uint16_t VersionNeeded = 20;

        // Names are UTF-8. Bit 11 is what says so; without it a name with anything but ASCII
        // in it is read as the OEM code page and comes out as mojibake.
        constexpr uint16_t FlagUtf8Names = 0x0800;

        void Put16(_Inout_ std::vector<uint8_t>& bytes, _In_ uint16_t value) noexcept
        {
            bytes.push_back(static_cast<uint8_t>(value & 0xFF));
            bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void Put32(_Inout_ std::vector<uint8_t>& bytes, _In_ uint32_t value) noexcept
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
            }
        }

        uint16_t Read16(_In_reads_(2) uint8_t const* at) noexcept
        {
            return static_cast<uint16_t>(at[0] | (at[1] << 8));
        }

        uint32_t Read32(_In_reads_(4) uint8_t const* at) noexcept
        {
            return static_cast<uint32_t>(at[0]) |
                (static_cast<uint32_t>(at[1]) << 8) |
                (static_cast<uint32_t>(at[2]) << 16) |
                (static_cast<uint32_t>(at[3]) << 24);
        }

        uint32_t Crc32(_In_ std::vector<uint8_t> const& bytes) noexcept
        {
            static uint32_t table[256]{};
            static bool built{ false };

            if (!built)
            {
                for (uint32_t i = 0; i < 256; ++i)
                {
                    auto value = i;

                    for (int bit = 0; bit < 8; ++bit)
                    {
                        value = (value & 1) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
                    }

                    table[i] = value;
                }

                built = true;
            }

            uint32_t crc{ 0xFFFFFFFFu };

            for (auto const byte : bytes)
            {
                crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
            }

            return crc ^ 0xFFFFFFFFu;
        }

        std::string ToUtf8(_In_ std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const needed = ::WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (needed <= 0)
            {
                return {};
            }

            std::string utf8(static_cast<size_t>(needed), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                utf8.data(), needed, nullptr, nullptr);

            return utf8;
        }

        std::wstring FromUtf8(_In_ std::string const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const needed = ::MultiByteToWideChar(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);

            if (needed <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(needed), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), needed);

            return wide;
        }

        std::vector<uint8_t> ReadAllBytes(_In_ std::filesystem::path const& path) noexcept
        {
            try
            {
                std::ifstream file{ path, std::ios::binary };

                if (!file)
                {
                    return {};
                }

                return std::vector<uint8_t>{
                    std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
            }
            catch (...)
            {
                return {};
            }
        }

        bool WriteAllBytes(
            _In_ std::filesystem::path const& path,
            _In_ std::vector<uint8_t> const& bytes) noexcept
        {
            try
            {
                std::ofstream file{ path, std::ios::binary | std::ios::trunc };

                if (!file)
                {
                    return false;
                }

                if (!bytes.empty())
                {
                    file.write(
                        reinterpret_cast<char const*>(bytes.data()),
                        static_cast<std::streamsize>(bytes.size()));
                }

                return file.good();
            }
            catch (...)
            {
                return false;
            }
        }

        struct PackedFile
        {
            std::wstring Name{};
            std::vector<uint8_t> Bytes{};
        };

        std::vector<uint8_t> BuildZip(_In_ std::vector<PackedFile> const& files) noexcept
        {
            std::vector<uint8_t> zip{};
            std::vector<uint8_t> directory{};

            uint32_t count{ 0 };

            for (auto const& file : files)
            {
                auto const name = ToUtf8(file.Name);
                auto const crc = Crc32(file.Bytes);
                auto const size = static_cast<uint32_t>(file.Bytes.size());
                auto const offset = static_cast<uint32_t>(zip.size());

                Put32(zip, LocalHeaderSignature);
                Put16(zip, VersionNeeded);
                Put16(zip, FlagUtf8Names);
                Put16(zip, MethodStored);

                // No timestamp. A backup's time is the file's own, and a package that changes
                // its bytes every time it is written cannot be compared against the last one.
                Put16(zip, 0);
                Put16(zip, 0);

                Put32(zip, crc);
                Put32(zip, size);
                Put32(zip, size);
                Put16(zip, static_cast<uint16_t>(name.size()));
                Put16(zip, 0);

                zip.insert(zip.end(), name.begin(), name.end());
                zip.insert(zip.end(), file.Bytes.begin(), file.Bytes.end());

                Put32(directory, CentralHeaderSignature);
                Put16(directory, VersionNeeded);
                Put16(directory, VersionNeeded);
                Put16(directory, FlagUtf8Names);
                Put16(directory, MethodStored);
                Put16(directory, 0);
                Put16(directory, 0);
                Put32(directory, crc);
                Put32(directory, size);
                Put32(directory, size);
                Put16(directory, static_cast<uint16_t>(name.size()));
                Put16(directory, 0);
                Put16(directory, 0);
                Put16(directory, 0);
                Put16(directory, 0);
                Put32(directory, 0);
                Put32(directory, offset);

                directory.insert(directory.end(), name.begin(), name.end());

                count++;
            }

            auto const directoryOffset = static_cast<uint32_t>(zip.size());

            zip.insert(zip.end(), directory.begin(), directory.end());

            Put32(zip, EndOfDirectorySignature);
            Put16(zip, 0);
            Put16(zip, 0);
            Put16(zip, static_cast<uint16_t>(count));
            Put16(zip, static_cast<uint16_t>(count));
            Put32(zip, static_cast<uint32_t>(directory.size()));
            Put32(zip, directoryOffset);
            Put16(zip, 0);

            return zip;
        }

        // Walks the local headers rather than the central directory. Both describe the same
        // entries; the headers are what the bytes actually follow, so reading them cannot be
        // led somewhere else by a directory that disagrees with them.
        bool ReadZip(
            _In_ std::vector<uint8_t> const& zip,
            _Out_ std::vector<PackedFile>& files,
            _Out_ bool& compressed) noexcept
        {
            files.clear();
            compressed = false;

            size_t at{ 0 };

            while (at + 30 <= zip.size())
            {
                if (Read32(&zip[at]) != LocalHeaderSignature)
                {
                    break;
                }

                auto const method = Read16(&zip[at + 8]);
                auto const size = Read32(&zip[at + 18]);
                auto const nameLength = Read16(&zip[at + 26]);
                auto const extraLength = Read16(&zip[at + 28]);

                auto const nameAt = at + 30;
                auto const dataAt = nameAt + nameLength + extraLength;

                if (dataAt > zip.size() || dataAt + size > zip.size())
                {
                    return false;
                }

                if (method != MethodStored)
                {
                    compressed = true;
                    return false;
                }

                if (files.size() >= MaximumPackageEntries)
                {
                    return false;
                }

                PackedFile file{};

                file.Name = FromUtf8(std::string{
                    reinterpret_cast<char const*>(&zip[nameAt]), nameLength });

                file.Bytes.assign(zip.begin() + dataAt, zip.begin() + dataAt + size);

                files.push_back(std::move(file));

                at = dataAt + size;
            }

            return !files.empty();
        }

        // Every file a layout points at, as bare names. Only the layout's own folder is ever
        // read from, so a name that tries to climb out of it resolves to nothing and is left.
        std::vector<std::wstring> PicturesOf(_In_ LayoutDocument const& document) noexcept
        {
            std::set<std::wstring> names{};

            if (!document.BackgroundImage.empty())
            {
                names.insert(document.BackgroundImage);
            }

            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    if (!control.Image.FileName.empty())
                    {
                        names.insert(control.Image.FileName);
                    }
                }
            }

            return { names.begin(), names.end() };
        }

        std::wstring LayoutStemOf(_In_ std::wstring const& layoutFilePath) noexcept
        {
            auto stem = std::filesystem::path{ layoutFilePath }.filename().wstring();

            // ".midilayout.json" is two extensions, so `stem()` only takes one of them off.
            auto const dot = stem.find(L'.');

            return dot == std::wstring::npos ? stem : stem.substr(0, dot);
        }
    }

    std::wstring BackupsFolderPath() noexcept
    {
        try
        {
            auto const layouts = LayoutsFolder();

            if (layouts.empty())
            {
                return {};
            }

            auto const folder = std::filesystem::path{ layouts } / L"Backups";

            std::error_code ignored{};

            std::filesystem::create_directories(folder, ignored);

            return std::filesystem::is_directory(folder, ignored) ? folder.wstring() : std::wstring{};
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::vector<BackupEntry> ListBackups(std::wstring const& layoutFilePath) noexcept
    {
        std::vector<BackupEntry> backups{};

        try
        {
            auto const folder = BackupsFolderPath();

            if (folder.empty() || layoutFilePath.empty())
            {
                return backups;
            }

            auto const stem = LayoutStemOf(layoutFilePath);
            auto const prefix = stem + L" backup ";

            std::error_code ignored{};

            for (auto const& entry : std::filesystem::directory_iterator{ folder, ignored })
            {
                if (!entry.is_regular_file(ignored))
                {
                    continue;
                }

                auto const name = entry.path().filename().wstring();

                if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
                {
                    continue;
                }

                if (entry.path().extension().wstring() != LayoutPackageExtension)
                {
                    continue;
                }

                auto const digits = name.substr(
                    prefix.size(), name.size() - prefix.size() - wcslen(LayoutPackageExtension));

                if (digits.empty() ||
                    digits.find_first_not_of(L"0123456789") != std::wstring::npos)
                {
                    continue;
                }

                BackupEntry backup{};

                backup.Path = entry.path().wstring();
                backup.Number = std::stoi(digits);
                backup.Bytes = static_cast<uint64_t>(entry.file_size(ignored));

                if (auto const written = entry.last_write_time(ignored); !ignored)
                {
                    backup.WrittenAt = written.time_since_epoch().count();
                }

                backups.push_back(std::move(backup));
            }

            std::sort(backups.begin(), backups.end(),
                [](BackupEntry const& left, BackupEntry const& right)
                { return left.Number > right.Number; });
        }
        catch (...)
        {
            backups.clear();
        }

        return backups;
    }

    _Use_decl_annotations_
    std::wstring NextBackupPath(std::wstring const& layoutFilePath) noexcept
    {
        try
        {
            auto const folder = BackupsFolderPath();

            if (folder.empty() || layoutFilePath.empty())
            {
                return {};
            }

            auto const existing = ListBackups(layoutFilePath);
            auto const next = existing.empty() ? 1 : existing.front().Number + 1;

            wchar_t digits[8]{};
            swprintf_s(digits, L"%03d", std::clamp(next, 1, 999));

            auto const name = LayoutStemOf(layoutFilePath) + L" backup " + digits +
                LayoutPackageExtension;

            return (std::filesystem::path{ folder } / name).wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    PackageSurvey SurveyLayoutPackage(std::wstring const& layoutFilePath) noexcept
    {
        PackageSurvey survey{};

        try
        {
            std::error_code ignored{};

            if (layoutFilePath.empty() ||
                !std::filesystem::is_regular_file(layoutFilePath, ignored))
            {
                return survey;
            }

            auto const read = ReadLayoutFile(layoutFilePath);

            if (!read.Succeeded)
            {
                return survey;
            }

            auto const note = [&survey](std::wstring const& name, uint64_t bytes)
                {
                    survey.FileCount++;
                    survey.TotalBytes += bytes;

                    if (IsVideoFileName(name))
                    {
                        survey.VideoCount++;
                        survey.VideoBytes += bytes;
                    }

                    if (bytes > survey.LargestBytes)
                    {
                        survey.LargestBytes = bytes;
                        survey.LargestName = name;
                    }
                };

            auto const layoutPath = std::filesystem::path{ layoutFilePath };

            note(layoutPath.filename().wstring(),
                static_cast<uint64_t>(std::filesystem::file_size(layoutPath, ignored)));

            auto const folder = layoutPath.parent_path();

            // The same rules the writer uses, so the two cannot disagree about what travels.
            for (auto const& name : PicturesOf(read.Document))
            {
                if (SanitizeFileName(name).empty())
                {
                    continue;
                }

                auto const path = folder / name;

                if (!std::filesystem::is_regular_file(path, ignored))
                {
                    continue;
                }

                note(name, static_cast<uint64_t>(std::filesystem::file_size(path, ignored)));
            }

            survey.TooBig = survey.TotalBytes > MaximumWritablePackageBytes;
            survey.FitsWithoutVideo =
                survey.VideoCount > 0 && survey.BytesWithoutVideo() <= MaximumWritablePackageBytes;
        }
        catch (...)
        {
            survey = PackageSurvey{};
        }

        return survey;
    }

    _Use_decl_annotations_
    PackageResult WriteLayoutPackage(
        std::wstring const& layoutFilePath,
        std::wstring const& packagePath,
        bool includeVideo) noexcept
    {
        PackageResult result{};

        try
        {
            if (layoutFilePath.empty() || packagePath.empty())
            {
                result.FailureKey = L"PackageFailedNoLayout";
                return result;
            }

            std::error_code ignored{};

            if (!std::filesystem::is_regular_file(layoutFilePath, ignored))
            {
                result.FailureKey = L"PackageFailedNoLayout";
                return result;
            }

            auto const read = ReadLayoutFile(layoutFilePath);

            if (!read.Succeeded)
            {
                result.FailureKey = L"PackageFailedUnreadable";
                return result;
            }

            // Before a byte is read. The writer builds the whole zip in memory, so finding out
            // afterwards would mean a multi gigabyte spike and then a refusal.
            auto const survey = SurveyLayoutPackage(layoutFilePath);

            if (includeVideo ? survey.TooBig
                             : survey.BytesWithoutVideo() > MaximumWritablePackageBytes)
            {
                result.FailureKey = L"PackageFailedTooBig";
                return result;
            }

            std::vector<PackedFile> files{};

            PackedFile layout{};

            layout.Name = std::filesystem::path{ layoutFilePath }.filename().wstring();
            layout.Bytes = ReadAllBytes(layoutFilePath);

            if (layout.Bytes.empty())
            {
                result.FailureKey = L"PackageFailedUnreadable";
                return result;
            }

            files.push_back(std::move(layout));

            auto const folder = std::filesystem::path{ layoutFilePath }.parent_path();

            for (auto const& name : PicturesOf(read.Document))
            {
                // Only a bare name, only from the layout's own folder. A picture that is not
                // there is left out rather than refused: a layout with one broken reference is
                // still worth backing up.
                if (SanitizeFileName(name).empty())
                {
                    continue;
                }

                if (!includeVideo && IsVideoFileName(name))
                {
                    continue;
                }

                auto const path = folder / name;

                if (!std::filesystem::is_regular_file(path, ignored))
                {
                    continue;
                }

                PackedFile picture{};

                picture.Name = name;
                picture.Bytes = ReadAllBytes(path);

                if (!picture.Bytes.empty())
                {
                    files.push_back(std::move(picture));
                }
            }

            auto const zip = BuildZip(files);

            if (zip.size() > MaximumWritablePackageBytes)
            {
                result.FailureKey = L"PackageFailedTooBig";
                return result;
            }

            std::filesystem::create_directories(
                std::filesystem::path{ packagePath }.parent_path(), ignored);

            if (!WriteAllBytes(packagePath, zip))
            {
                result.FailureKey = L"PackageFailedWrite";
                return result;
            }

            result.Succeeded = true;
            result.Path = packagePath;
            result.FileCount = static_cast<uint32_t>(files.size());
        }
        catch (...)
        {
            result.Succeeded = false;
            result.FailureKey = L"PackageFailedWrite";
        }

        return result;
    }

    _Use_decl_annotations_
    PackageResult ReadLayoutPackage(
        std::wstring const& packagePath,
        std::wstring const& targetFolder) noexcept
    {
        PackageResult result{};

        try
        {
            std::error_code ignored{};

            if (packagePath.empty() || targetFolder.empty() ||
                !std::filesystem::is_regular_file(packagePath, ignored))
            {
                result.FailureKey = L"ImportFailedUnreadable";
                return result;
            }

            if (std::filesystem::file_size(packagePath, ignored) > MaximumPackageBytes)
            {
                result.FailureKey = L"PackageFailedTooBig";
                return result;
            }

            auto const bytes = ReadAllBytes(packagePath);

            std::vector<PackedFile> files{};
            auto compressed = false;

            if (!ReadZip(bytes, files, compressed))
            {
                result.FailureKey = compressed ? L"ImportFailedCompressed" : L"ImportFailedUnreadable";
                return result;
            }

            // Exactly one layout, and it decides what everything else is named around.
            PackedFile const* layout{ nullptr };

            for (auto const& file : files)
            {
                if (file.Name.size() > wcslen(LayoutFileExtension) &&
                    file.Name.compare(
                        file.Name.size() - wcslen(LayoutFileExtension),
                        wcslen(LayoutFileExtension),
                        LayoutFileExtension) == 0)
                {
                    layout = &file;
                    break;
                }
            }

            if (layout == nullptr)
            {
                result.FailureKey = L"ImportFailedNoLayout";
                return result;
            }

            std::filesystem::create_directories(targetFolder, ignored);

            // The name inside the package, unless it is taken. Writing over a layout somebody
            // already has because a friend's copy happened to share its name is not a thing
            // this app gets to do.
            auto layoutPath = std::filesystem::path{ targetFolder } / layout->Name;

            if (std::filesystem::exists(layoutPath, ignored))
            {
                auto const stem = LayoutStemOf(layout->Name);

                layoutPath = MakeUnusedLayoutPath(targetFolder, stem);
            }

            if (layoutPath.empty())
            {
                result.FailureKey = L"ImportFailedWrite";
                return result;
            }

            // The pictures first: a layout written before its artwork would draw empty boxes if
            // anything went wrong halfway.
            std::vector<std::pair<std::wstring, std::wstring>> renamed{};

            for (auto const& file : files)
            {
                if (&file == layout)
                {
                    continue;
                }

                // A name that is a path, or which climbs out with "..", never reaches the disk.
                auto const safe = SanitizeFileName(file.Name);

                if (safe.empty() || !IsSupportedPictureFileName(safe))
                {
                    continue;
                }

                auto target = std::filesystem::path{ targetFolder } / safe;

                for (int32_t attempt = 2;
                    std::filesystem::exists(target, ignored) && attempt < 1000;
                    ++attempt)
                {
                    auto const stem = std::filesystem::path{ safe }.stem().wstring() +
                        L" " + std::to_wstring(attempt);

                    target = std::filesystem::path{ targetFolder } /
                        (stem + std::filesystem::path{ safe }.extension().wstring());
                }

                if (!WriteAllBytes(target, file.Bytes))
                {
                    continue;
                }

                if (target.filename().wstring() != safe)
                {
                    renamed.emplace_back(safe, target.filename().wstring());
                }

                result.FileCount++;
            }

            // A picture that had to be renamed around one already there takes its new name with
            // it into the layout, or the imported copy points at somebody else's artwork.
            if (renamed.empty())
            {
                if (!WriteAllBytes(layoutPath, layout->Bytes))
                {
                    result.FailureKey = L"ImportFailedWrite";
                    return result;
                }
            }
            else
            {
                // The bytes in the package are the layout's own text, so they are turned back
                // into a document, the renamed pictures are pointed at, and it is written
                // through the ordinary writer.
                auto const json = FromUtf8(std::string{
                    reinterpret_cast<char const*>(layout->Bytes.data()), layout->Bytes.size() });

                auto parsed = ReadLayoutFromJson(json);

                if (!parsed.Succeeded)
                {
                    result.FailureKey = L"ImportFailedUnreadable";
                    return result;
                }

                auto document = std::move(parsed.Document);

                auto const rename = [&renamed](std::wstring& name)
                    {
                        for (auto const& [was, now] : renamed)
                        {
                            if (name == was)
                            {
                                name = now;
                                return;
                            }
                        }
                    };

                rename(document.BackgroundImage);

                for (auto& page : document.Pages)
                {
                    for (auto& control : page.Controls)
                    {
                        rename(control.Image.FileName);
                    }
                }

                document.FilePath = layoutPath.wstring();

                if (!WriteLayoutFile(document, document.FilePath))
                {
                    result.FailureKey = L"ImportFailedWrite";
                    return result;
                }
            }

            result.FileCount++;
            result.Succeeded = true;
            result.Path = layoutPath.wstring();
        }
        catch (...)
        {
            result.Succeeded = false;
            result.FailureKey = L"ImportFailedWrite";
        }

        return result;
    }

    _Use_decl_annotations_
    PackageResult RestoreLayoutFromBackup(
        std::wstring const& backupPath,
        std::wstring const& layoutFilePath) noexcept
    {
        PackageResult result{};

        try
        {
            std::error_code ignored{};

            if (backupPath.empty() || layoutFilePath.empty() ||
                !std::filesystem::is_regular_file(backupPath, ignored))
            {
                result.FailureKey = L"RestoreFailedNoBackup";
                return result;
            }

            // What is there now is backed up first. Restoring the wrong one has to be
            // recoverable, or nobody will ever press the button.
            //
            // Without the video: this copy exists to protect the layout being replaced, and a
            // restore only writes the files the backup actually holds. Duplicating gigabytes of
            // clip on every restore, to guard against a clip that is still sitting in the
            // folder either way, is not worth the disk.
            if (std::filesystem::is_regular_file(layoutFilePath, ignored))
            {
                WriteLayoutPackage(layoutFilePath, NextBackupPath(layoutFilePath), false);
            }

            auto const bytes = ReadAllBytes(backupPath);

            std::vector<PackedFile> files{};
            auto compressed = false;

            if (!ReadZip(bytes, files, compressed))
            {
                result.FailureKey = compressed ? L"ImportFailedCompressed" : L"ImportFailedUnreadable";
                return result;
            }

            auto const folder = std::filesystem::path{ layoutFilePath }.parent_path();

            for (auto const& file : files)
            {
                auto const safe = SanitizeFileName(file.Name);

                if (safe.empty())
                {
                    continue;
                }

                auto const isLayout =
                    safe.size() > wcslen(LayoutFileExtension) &&
                    safe.compare(
                        safe.size() - wcslen(LayoutFileExtension),
                        wcslen(LayoutFileExtension),
                        LayoutFileExtension) == 0;

                if (!isLayout && !IsSupportedPictureFileName(safe))
                {
                    continue;
                }

                // The layout goes back to the path it was restored onto, whatever it was
                // called inside the backup, so a renamed layout still restores onto itself.
                auto const target = isLayout
                    ? std::filesystem::path{ layoutFilePath }
                    : folder / safe;

                if (WriteAllBytes(target, file.Bytes))
                {
                    result.FileCount++;
                }
            }

            if (result.FileCount == 0)
            {
                result.FailureKey = L"ImportFailedWrite";
                return result;
            }

            result.Succeeded = true;
            result.Path = layoutFilePath;
        }
        catch (...)
        {
            result.Succeeded = false;
            result.FailureKey = L"ImportFailedWrite";
        }

        return result;
    }
}
