// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "LayoutStore.h"

#include <windows.h>
#include <shlobj_core.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace glass
{
    namespace
    {
        constexpr uint8_t Utf8Bom[] = { 0xEF, 0xBB, 0xBF };

        std::wstring FromUtf8(_In_ std::string const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const needed = ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);

            if (needed <= 0)
            {
                return {};
            }

            std::wstring result(static_cast<size_t>(needed), L'\0');

            ::MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), needed);

            return result;
        }

        std::string ToUtf8(_In_ std::wstring const& text) noexcept
        {
            if (text.empty())
            {
                return {};
            }

            auto const needed = ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

            if (needed <= 0)
            {
                return {};
            }

            std::string result(static_cast<size_t>(needed), '\0');

            ::WideCharToMultiByte(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), needed, nullptr, nullptr);

            return result;
        }
    }

    std::wstring LayoutsFolder() noexcept
    {
        try
        {
            PWSTR raw{ nullptr };

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &raw)))
            {
                return {};
            }

            std::filesystem::path folder{ raw };
            ::CoTaskMemFree(raw);

            folder /= LayoutFolderName;

            std::error_code ignored{};
            std::filesystem::create_directories(folder, ignored);

            return folder.wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool IsInLayoutsFolder(std::wstring const& filePath) noexcept
    {
        try
        {
            auto const folder = LayoutsFolder();

            if (folder.empty() || filePath.empty())
            {
                return false;
            }

            std::error_code ignored{};

            auto const parent = std::filesystem::weakly_canonical(
                std::filesystem::path{ filePath }.parent_path(), ignored);

            auto const expected = std::filesystem::weakly_canonical(
                std::filesystem::path{ folder }, ignored);

            return !parent.empty() && parent == expected;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring MakeUnusedLayoutPath(
        std::wstring const& folder,
        std::wstring const& layoutName) noexcept
    {
        try
        {
            if (folder.empty())
            {
                return {};
            }

            std::wstring fileName{};

            for (auto const character : layoutName)
            {
                fileName += (character < 32 || ::wcschr(L"\\/:*?\"<>|", character) != nullptr)
                    ? L'-'
                    : character;
            }

            // Windows will not keep a trailing space or dot on a file name, so trimming here
            // means the name on disk is the name that was asked for.
            while (!fileName.empty() && (fileName.back() == L' ' || fileName.back() == L'.'))
            {
                fileName.pop_back();
            }

            if (fileName.empty())
            {
                fileName = L"Layout";
            }

            auto path = folder + L"\\" + fileName + LayoutFileExtension;

            std::error_code ignored{};

            for (int32_t attempt = 2;
                std::filesystem::exists(path, ignored) && attempt < 1000;
                ++attempt)
            {
                path = folder + L"\\" + fileName + L" " + std::to_wstring(attempt) + LayoutFileExtension;
            }

            return path;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring BackgroundImagePath(LayoutDocument const& document) noexcept
    {
        try
        {
            if (document.BackgroundImage.empty() || document.FilePath.empty())
            {
                return {};
            }

            // Only a plain file name is ever accepted. A layout is untrusted input, so a name
            // that is a path is a way to make this app open a file somewhere else on the PC.
            std::filesystem::path const name{ document.BackgroundImage };

            if (name.has_parent_path() || name.has_root_name() || !name.has_filename())
            {
                return {};
            }

            auto const folder = std::filesystem::path{ document.FilePath }.parent_path();
            auto const full = folder / name;

            std::error_code ignored{};

            if (!std::filesystem::is_regular_file(full, ignored))
            {
                return {};
            }

            return full.wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool BackgroundImageNeedsCopying(
        std::wstring const& sourcePath,
        std::wstring const& layoutFilePath) noexcept
    {
        try
        {
            if (sourcePath.empty() || layoutFilePath.empty())
            {
                return false;
            }

            auto const source = std::filesystem::path{ sourcePath }.parent_path();
            auto const layout = std::filesystem::path{ layoutFilePath }.parent_path();

            std::error_code ignored{};

            return !std::filesystem::equivalent(source, layout, ignored);
        }
        catch (...)
        {
            return true;
        }
    }

    _Use_decl_annotations_
    std::wstring CopyBackgroundImageBeside(
        std::wstring const& sourcePath,
        std::wstring const& layoutFilePath) noexcept
    {
        try
        {
            if (sourcePath.empty() || layoutFilePath.empty())
            {
                return {};
            }

            std::filesystem::path const source{ sourcePath };

            std::error_code ignored{};

            if (!std::filesystem::is_regular_file(source, ignored))
            {
                return {};
            }

            auto const folder = std::filesystem::path{ layoutFilePath }.parent_path();

            if (!BackgroundImageNeedsCopying(sourcePath, layoutFilePath))
            {
                return source.filename().wstring();
            }

            auto target = folder / source.filename();

            // A different picture that wants a taken name gets a new one rather than writing
            // over something the customer put there.
            for (int32_t attempt = 2;
                std::filesystem::exists(target, ignored) && attempt < 1000;
                ++attempt)
            {
                auto stem = source.stem().wstring() + L" " + std::to_wstring(attempt);
                target = folder / (stem + source.extension().wstring());
            }

            std::filesystem::copy_file(
                source, target, std::filesystem::copy_options::overwrite_existing, ignored);

            if (ignored)
            {
                return {};
            }

            return target.filename().wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring ControlPicturePath(
        std::wstring const& layoutFilePath,
        std::wstring const& fileName) noexcept
    {
        try
        {
            if (fileName.empty() || layoutFilePath.empty())
            {
                return {};
            }

            // Only a plain file name is ever accepted. A layout is untrusted input, so a name
            // that is a path is a way to make this app open a file somewhere else on the PC.
            std::filesystem::path const name{ fileName };

            if (name.has_parent_path() || name.has_root_name() || !name.has_filename())
            {
                return {};
            }

            if (!IsSupportedPictureFileName(fileName))
            {
                return {};
            }

            auto const folder = std::filesystem::path{ layoutFilePath }.parent_path();
            auto const full = folder / name;

            std::error_code ignored{};

            if (!std::filesystem::is_regular_file(full, ignored))
            {
                return {};
            }

            return full.wstring();
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool IsSupportedPictureFileName(std::wstring const& fileName) noexcept
    {
        try
        {
            auto const extension = std::filesystem::path{ fileName }.extension().wstring();

            for (auto const* const known : { L".png", L".jpg", L".jpeg", L".svg" })
            {
                if (_wcsicmp(extension.c_str(), known) == 0)
                {
                    return true;
                }
            }

            return IsVideoFileName(fileName);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool IsVideoFileName(std::wstring const& fileName) noexcept
    {
        try
        {
            auto const extension = std::filesystem::path{ fileName }.extension().wstring();

            for (auto const* const known : { L".mp4", L".m4v", L".mkv", L".webm", L".wmv" })
            {
                if (_wcsicmp(extension.c_str(), known) == 0)
                {
                    return true;
                }
            }

            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    ReadResult ReadLayoutFile(std::wstring const& filePath) noexcept
    {
        ReadResult result{};

        try
        {
            std::error_code ec{};

            auto const size = std::filesystem::file_size(std::filesystem::path{ filePath }, ec);

            if (ec)
            {
                result.Detail = L"The file could not be opened.";
                return result;
            }

            // Bounded before a single byte is read into memory. A layout can arrive from a
            // stranger, and a multi-gigabyte "layout" is the cheapest attack there is.
            if (size > MaximumLayoutFileBytes)
            {
                result.Detail = L"The file is larger than a layout is allowed to be.";
                return result;
            }

            std::ifstream stream{ std::filesystem::path{ filePath }, std::ios::binary };

            if (!stream.is_open())
            {
                result.Detail = L"The file could not be opened.";
                return result;
            }

            std::string bytes(static_cast<size_t>(size), '\0');
            stream.read(bytes.data(), static_cast<std::streamsize>(size));
            stream.close();

            if (bytes.size() >= sizeof(Utf8Bom) &&
                static_cast<uint8_t>(bytes[0]) == Utf8Bom[0] &&
                static_cast<uint8_t>(bytes[1]) == Utf8Bom[1] &&
                static_cast<uint8_t>(bytes[2]) == Utf8Bom[2])
            {
                bytes.erase(0, sizeof(Utf8Bom));
            }

            result = ReadLayoutFromJson(FromUtf8(bytes));

            if (result.Succeeded)
            {
                result.Document.FilePath = filePath;
                result.Document.IsImported = !IsInLayoutsFolder(filePath);
            }
        }
        catch (...)
        {
            result.Succeeded = false;
            result.Detail = L"The file could not be read.";
        }

        return result;
    }

    _Use_decl_annotations_
    bool WriteLayoutFile(LayoutDocument const& document, std::wstring const& filePath) noexcept
    {
        try
        {
            auto const text = WriteLayoutToJson(document);

            if (text.empty() || filePath.empty())
            {
                return false;
            }

            auto const folder = std::filesystem::path{ filePath }.parent_path();

            if (!folder.empty())
            {
                std::error_code ignored{};
                std::filesystem::create_directories(folder, ignored);
            }

            // Written beside the target and moved into place, so a failure half way through
            // leaves the previous layout intact rather than a truncated one.
            auto temporary = std::filesystem::path{ filePath };
            temporary += L".writing";

            {
                std::ofstream stream{ temporary, std::ios::binary | std::ios::trunc };

                if (!stream.is_open())
                {
                    return false;
                }

                auto const bytes = ToUtf8(text);
                stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

                if (!stream.good())
                {
                    stream.close();
                    std::error_code ignored{};
                    std::filesystem::remove(temporary, ignored);
                    return false;
                }
            }

            std::error_code ec{};
            std::filesystem::rename(temporary, std::filesystem::path{ filePath }, ec);

            if (ec)
            {
                std::filesystem::remove(temporary, ec);
                return false;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::vector<std::wstring> ListLayoutFiles() noexcept
    {
        std::vector<std::wstring> files{};

        try
        {
            auto const folder = LayoutsFolder();

            if (folder.empty())
            {
                return files;
            }

            std::error_code ec{};

            for (auto const& entry : std::filesystem::directory_iterator{ folder, ec })
            {
                if (!entry.is_regular_file(ec))
                {
                    continue;
                }

                auto const name = entry.path().filename().wstring();

                if (name.size() <= std::size(LayoutFileExtension) - 1)
                {
                    continue;
                }

                auto const tail = name.substr(name.size() - (std::size(LayoutFileExtension) - 1));

                if (::CompareStringOrdinal(tail.c_str(), -1, LayoutFileExtension, -1, TRUE) == CSTR_EQUAL)
                {
                    files.push_back(entry.path().wstring());
                }
            }

            std::sort(files.begin(), files.end());
        }
        catch (...)
        {
        }

        return files;
    }
}
