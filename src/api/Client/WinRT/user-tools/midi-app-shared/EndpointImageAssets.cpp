// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "EndpointImageAssets.h"

#include <shlwapi.h>
#include <shlobj_core.h>
#include <shobjidl_core.h>

namespace midiapp
{
    namespace
    {
        constexpr wchar_t AssetsFolder[] = LR"(%allusersprofile%\Microsoft\MIDI\Assets\Endpoints)";
        constexpr wchar_t FileNamePrefix[] = L"ep-";

        // matches the settings app, which gives up rather than spin forever on a busy folder
        constexpr int32_t MaximumNameAttempts = 500;

        // wstring_util.h cannot be included here: it uses bare min() and these apps define
        // NOMINMAX. This is the same rule its CleanImageFileName applies, kept in step by hand.
        std::wstring CleanFileName(_In_ std::wstring const& fileName) noexcept
        {
            try
            {
                std::wstring result{ fileName };

                auto const first = result.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                result = result.substr(first, result.find_last_not_of(L" \t\r\n") - first + 1);

                // keep only the last component, so a stored path cannot walk out of the folder
                auto const lastSeparator = result.find_last_of(L"\\/");

                if (lastSeparator != std::wstring::npos)
                {
                    result = result.substr(lastSeparator + 1);
                }

                if (result.empty() || result.find_first_not_of(L'.') == std::wstring::npos)
                {
                    return {};
                }

                for (auto const ch : result)
                {
                    if (ch < 0x20 || ch == L':' || ch == L'<' || ch == L'>' ||
                        ch == L'"' || ch == L'|' || ch == L'?' || ch == L'*')
                    {
                        return {};
                    }
                }

                return result;
            }
            catch (...)
            {
                return {};
            }
        }

        bool FilesAreIdentical(_In_ std::wstring const& left, _In_ std::wstring const& right) noexcept
        {
            try
            {
                wil::unique_hfile leftFile{ ::CreateFileW(
                    left.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr) };

                wil::unique_hfile rightFile{ ::CreateFileW(
                    right.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr) };

                if (!leftFile || !rightFile)
                {
                    return false;
                }

                LARGE_INTEGER leftSize{};
                LARGE_INTEGER rightSize{};

                if (!::GetFileSizeEx(leftFile.get(), &leftSize) ||
                    !::GetFileSizeEx(rightFile.get(), &rightSize) ||
                    leftSize.QuadPart != rightSize.QuadPart)
                {
                    return false;
                }

                std::vector<uint8_t> leftBuffer(4096);
                std::vector<uint8_t> rightBuffer(4096);

                for (;;)
                {
                    DWORD leftRead{ 0 };
                    DWORD rightRead{ 0 };

                    if (!::ReadFile(leftFile.get(), leftBuffer.data(), static_cast<DWORD>(leftBuffer.size()), &leftRead, nullptr) ||
                        !::ReadFile(rightFile.get(), rightBuffer.data(), static_cast<DWORD>(rightBuffer.size()), &rightRead, nullptr))
                    {
                        return false;
                    }

                    if (leftRead != rightRead)
                    {
                        return false;
                    }

                    if (leftRead == 0)
                    {
                        return true;
                    }

                    if (memcmp(leftBuffer.data(), rightBuffer.data(), leftRead) != 0)
                    {
                        return false;
                    }
                }
            }
            catch (...)
            {
                return false;
            }
        }

        bool EnsureFolder(_In_ std::wstring const& path) noexcept
        {
            auto const attributes = ::GetFileAttributesW(path.c_str());

            if (attributes != INVALID_FILE_ATTRIBUTES)
            {
                return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }

            return ::SHCreateDirectoryExW(nullptr, path.c_str(), nullptr) == ERROR_SUCCESS;
        }
    }

    std::wstring EndpointImageAssets::FolderPath() noexcept
    {
        try
        {
            wchar_t buffer[MAX_PATH]{};

            auto const written = ::ExpandEnvironmentStringsW(AssetsFolder, buffer, ARRAYSIZE(buffer));

            if (written == 0 || written > ARRAYSIZE(buffer))
            {
                return {};
            }

            return std::wstring{ buffer };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring EndpointImageAssets::FullPathForFileName(std::wstring const& fileName) noexcept
    {
        try
        {
            auto const cleaned = CleanFileName(fileName);
            auto const folder = FolderPath();

            if (cleaned.empty() || folder.empty())
            {
                return {};
            }

            return folder + L"\\" + cleaned;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    bool EndpointImageAssets::Exists(std::wstring const& fileName) noexcept
    {
        auto const path = FullPathForFileName(fileName);

        if (path.empty())
        {
            return false;
        }

        auto const attributes = ::GetFileAttributesW(path.c_str());

        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    _Use_decl_annotations_
    std::wstring EndpointImageAssets::CopyIntoFolder(
        std::wstring const& sourcePath,
        std::wstring& errorMessage) noexcept
    {
        errorMessage.clear();

        try
        {
            if (sourcePath.empty())
            {
                return {};
            }

            auto const folder = FolderPath();

            if (folder.empty() || !EnsureFolder(folder))
            {
                errorMessage = L"folder";
                return {};
            }

            auto const leaf = CleanFileName(sourcePath);

            if (leaf.empty())
            {
                errorMessage = L"name";
                return {};
            }

            auto const dot = leaf.find_last_of(L'.');
            auto const stem = dot == std::wstring::npos ? leaf : leaf.substr(0, dot);
            auto const extension = dot == std::wstring::npos ? std::wstring{} : leaf.substr(dot);

            std::wstring candidate{ FileNamePrefix + leaf };

            for (int32_t attempt = 1; ; attempt++)
            {
                auto const destination = folder + L"\\" + candidate;

                // picking the file that is already in the folder is a no-op, not a copy over itself
                if (::CompareStringOrdinal(
                        destination.c_str(), -1, sourcePath.c_str(), -1, TRUE) == CSTR_EQUAL)
                {
                    return candidate;
                }

                if (::GetFileAttributesW(destination.c_str()) == INVALID_FILE_ATTRIBUTES)
                {
                    if (::CopyFileW(sourcePath.c_str(), destination.c_str(), TRUE))
                    {
                        return candidate;
                    }

                    errorMessage = L"copy";
                    return {};
                }

                // the same picture chosen twice should not leave two copies behind
                if (FilesAreIdentical(sourcePath, destination))
                {
                    return candidate;
                }

                if (attempt > MaximumNameAttempts)
                {
                    errorMessage = L"name";
                    return {};
                }

                candidate = FileNamePrefix + stem + L" (" + std::to_wstring(attempt) + L")" + extension;
            }
        }
        catch (...)
        {
            errorMessage = L"copy";
            return {};
        }
    }

    _Use_decl_annotations_
    bool EndpointImageAssets::IsScalableVector(std::wstring const& fileName) noexcept
    {
        try
        {
            auto const dot = fileName.find_last_of(L'.');

            if (dot == std::wstring::npos)
            {
                return false;
            }

            auto const extension = fileName.substr(dot);

            return ::CompareStringOrdinal(extension.c_str(), -1, L".svg", -1, TRUE) == CSTR_EQUAL;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring EndpointImageAssets::ShowPicker(HWND const owner) noexcept
    {
        try
        {
            winrt::com_ptr<IFileOpenDialog> dialog{};

            if (FAILED(::CoCreateInstance(
                CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put()))))
            {
                return {};
            }

            COMDLG_FILTERSPEC const filters[]
            {
                { L"Pictures", L"*.png;*.jpg;*.jpeg;*.svg" },
            };

            LOG_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
            LOG_IF_FAILED(dialog->SetOptions(FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM));

            // Pictures is where a customer's artwork usually is, and this only applies the first
            // time: SetDefaultFolder defers to wherever they browsed to last.
            winrt::com_ptr<IShellItem> pictures{};

            if (SUCCEEDED(::SHCreateItemInKnownFolder(
                FOLDERID_Pictures, 0, nullptr, IID_PPV_ARGS(pictures.put()))))
            {
                LOG_IF_FAILED(dialog->SetDefaultFolder(pictures.get()));
            }

            // cancelling is reported as a failure hresult, so this is not logged as an error
            if (FAILED(dialog->Show(owner)))
            {
                return {};
            }

            winrt::com_ptr<IShellItem> item{};

            if (FAILED(dialog->GetResult(item.put())))
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
        catch (...)
        {
            return {};
        }
    }
}
